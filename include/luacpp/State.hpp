#ifndef LUACPP_STATE_HPP
#define LUACPP_STATE_HPP

#include "Basics.hpp"
#include "Table.hpp"
#include "Registry.hpp"
#include "Generic.hpp"
#include "Debug.hpp"
#include "Stack.hpp"
#include "StackGuard.hpp"
#include "ErrorHandling.hpp"
#include "WarningHandling.hpp"
#include "detail/Bind.hpp"
#include "detail/Config.hpp"
#include "detail/StateRegistry.hpp"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <filesystem>

#if LUACPP_HAS_SPAN
#include <span>
#endif

struct lua_State;

namespace Lua {

constexpr uint32_t bit(uint32_t n) { return 1 << n; }

// Semantic tag for file-source overloads of script-loading methods.
// Alias rather than a wrapper type so std::filesystem::path values pass
// through transparently without an extra construction step.
using File = std::filesystem::path;

class State {
public:
	using NativeFunction = Basics::NativeFunction;
	using Method = std::function<int(State&)>;
	using DebugHook = std::function<void(State&, const DebugInfo&)>;

	typedef std::function<void(Table&)> TableFunction;
	typedef uint32_t Library;

	// Library bits intentionally match Lua 5.5's LUA_*K constants (lualib.h)
	// so that the enum value can be passed directly to luaL_openselectedlibs.
	// Reordering here is a breaking change from the pre-5.5 layout.
	constexpr static const Library LibNone      = 0;
	constexpr static const Library LibBase      = bit(0); // LUA_GLIBK
	constexpr static const Library LibPackage   = bit(1); // LUA_LOADLIBK
	constexpr static const Library LibCoroutine = bit(2); // LUA_COLIBK
	constexpr static const Library LibDebug     = bit(3); // LUA_DBLIBK
	constexpr static const Library LibIO        = bit(4); // LUA_IOLIBK
	constexpr static const Library LibMath      = bit(5); // LUA_MATHLIBK
	constexpr static const Library LibOS        = bit(6); // LUA_OSLIBK
	constexpr static const Library LibString    = bit(7); // LUA_STRLIBK
	constexpr static const Library LibTable     = bit(8); // LUA_TABLIBK
	constexpr static const Library LibUTF8      = bit(9); // LUA_UTF8LIBK
	constexpr static const Library LibAll       = 0xFFFFFFFF;
	constexpr static size_t LibraryCount = 10;

	struct MetaTable {
		constexpr static const char* const Addition = "__add"; ///< addition operator (+)
		constexpr static const char* const Subtraction = "__sub"; ///< subtraction operator (-)
		constexpr static const char* const Multiplication = "__mul"; ///< multiplication operator (*)
		constexpr static const char* const Division = "__div"; ///< division operator (/)
		constexpr static const char* const Modulo = "__mod"; ///< modulo operator (%)
		constexpr static const char* const Power = "__pow"; ///< power operator (^)
		constexpr static const char* const UnaryMinus = "__unm"; ///< unary minus operator (-)
		constexpr static const char* const FloorDivision = "__idiv"; ///< floor division operator (//)
		constexpr static const char* const BitwiseAnd = "__band"; ///< bitwise and operator (&)
		constexpr static const char* const BitwiseOr = "__bor"; ///< bitwise or operator (|)
		constexpr static const char* const BitwiseXor = "__bxor"; ///< bitwise xor operator (~)
		constexpr static const char* const BitwiseNot = "__bnot"; ///< bitwise not operator (~)
		constexpr static const char* const LeftShift = "__shl"; ///< left shift operator (<<)
		constexpr static const char* const RightShift = "__shr"; ///< right shift operator (>>)
		constexpr static const char* const Concatenation = "__concat"; ///< concatenation operator (..)
		constexpr static const char* const Length = "__len"; ///< length operator (#)
		constexpr static const char* const Equal = "__eq"; ///< equal operator (==)
		constexpr static const char* const LessThan = "__lt"; ///< less than operator (<)
		constexpr static const char* const LessThanOrEqual = "__le"; ///< less than or equal operator (<=)
		constexpr static const char* const Index = "__index"; ///< index operator ([])
		constexpr static const char* const NewIndex = "__newindex"; ///< new index operator ([])
		constexpr static const char* const Call = "__call"; ///< call operator (function call)

		constexpr static const char* const Mode = "__mode"; ///< mode for weak tables
		constexpr static const char* const GC = "__gc"; ///< garbage collection
		constexpr static const char* const Tostring = "__tostring"; ///< tostring operator (tostring())
		constexpr static const char* const Name = "__name"; ///< name of the metatable
		constexpr static const char* const Close = "__close"; ///< close operator (close())
	};

	State(Library libraries = LibNone);
	State(lua_State* state);
	State(const State&) = delete;
	// Move is deleted on purpose. registerMethod() captures `this` as an
	// upvalue inside a Lua C-closure; once moved, every previously registered
	// closure still points at the moved-from State, so dispatching any Lua-
	// registered method would access a stale object. Fixing that correctly
	// would require re-binding every closure on move — out of scope for the
	// resource-owning API. Wrap in std::unique_ptr<State> if movability is
	// needed.
	State(State&&) = delete;
	State& operator=(const State&) = delete;
	State& operator=(State&&) = delete;

	~State();

	/**
	 * @brief Open the selected libraries immediately.
	 *
	 * Each set bit names a standard Lua library to open right now: its
	 * open-function runs and its global table becomes available.
	 *
	 * @note Backed by Lua 5.5's luaL_openselectedlibs; Library bits are
	 *       chosen to match LUA_*K so the value is forwarded directly.
	 */
	void openLibrary(Library library);

	/**
	 * @brief Register the selected libraries for lazy loading via require().
	 *
	 * Each set bit names a standard Lua library to add to package.preload.
	 * Scripts can then bring it in on demand with require("name"); until
	 * that call the library's globals are not present in the state.
	 *
	 * @note Requires LibPackage to be open (otherwise require() is not
	 *       reachable from scripts).
	 */
	void preloadLibrary(Library library);

	/**
	 * @brief Prepend a search pattern to Lua's module path.
	 *
	 * The pattern uses Lua's '?'-substitution to locate modules. For
	 * example, addModuleSearchPath("/opt/myapp/scripts/?.lua") makes
	 * require("foo") look for "/opt/myapp/scripts/foo.lua". The new
	 * pattern wins over existing ones because it is prepended.
	 *
	 * Typical use: extend Lua's defaults to find application-specific or
	 * non-standard LuaRocks tree locations.
	 *
	 * @param pattern         Lua module-search pattern (with '?').
	 * @param forNativeModule If true, modifies package.cpath (for native
	 *                        .so/.dll/.dylib modules); otherwise
	 *                        package.path (for .lua modules).
	 *
	 * @note Silent no-op when LibPackage is not open — without that
	 *       library there is no package table to modify.
	 */
	void addModuleSearchPath(const std::string& pattern, bool forNativeModule = false);


	/**
	 * \brief Read a global variable, returning nullopt when missing or of the
	 *        wrong type.
	 *
	 * Unlike the script-execution methods this does NOT invoke the configured
	 * error handler — "this global isn't there / isn't a T" is a query result,
	 * not a Lua-side error. Caller decides whether to treat nullopt as an error.
	 */
	template <typename T>
	[[nodiscard]] std::optional<T> readVariable(const char* variableName) {
		pushGlobalToStack(variableName);
		// tryGet never raises a Lua error (uses testUserData for class pointers),
		// so the plain popStack below always runs — no stack-cleanup hazard even
		// when the global has the wrong type. See Stack<T>::tryGet.
		std::optional<T> result = Stack<T>::tryGet(m_state, -1);
		popStack(1);
		return result;
	}

	template <typename T>
	void writeVariable(const char* variableName, T value) {
		pushToStack(value);
		setGlobalFromStack(variableName);
	}
	
	std::map<Generic, Generic> readTableGeneric(const char* tableName) {
		std::map<Generic, Generic> result;
		const Type t = pushGlobalToStack(tableName);
		DefaultStackGuard guard(m_state); // pops on every path, incl. readGeneric throwing
		if (t == Type::Table) {
			Table table(m_state, -1);
			result = table.readGeneric();
		}
		return result;
	}

	template <typename Key, typename Value>
	std::map<Key, Value> readTable(const char* tableName) {
		std::map<Key, Value> result;
		const Type t = pushGlobalToStack(tableName);
		DefaultStackGuard guard(m_state); // table.read can throw TypeMismatchException
		if (t == Type::Table) {
			Table table(m_state, -1);
			result = table.read<Key, Value>();
		}
		return result;
	}

	template <typename Key, typename Value>
	std::map<Key, Value> readTableIfMatching(const std::string& tableName) {
		std::map<Key, Value> result;
		const Type t = pushGlobalToStack(tableName.c_str());
		DefaultStackGuard guard(m_state);
		if (t == Type::Table) {
			Table table(m_state, -1);
			result = table.readIfMatching<Key, Value>();
		}
		return result;
	}

	template <typename T>
	void writeTable(const char* tableName, const std::map<std::string, T>& map) {
		withTableDo(tableName, [this, &map](Table& table) {
			table.write(map);
		}, true);
	}

	/**
	 * @brief Register a native function to be callable from Lua
	*/
	void registerNativeFunction(const char* name, NativeFunction func, int numUpValues = 0);

	/**
	 * @brief Register a native function with upvalues. The upvalues are pushed
	 *        onto the Lua stack and consumed by lua_pushcclosure.
	*/
	template <typename... Args>
	void registerNativeFunctionWithUpvalues(const char* name, NativeFunction func, Args... args) {
		(pushToStack(args), ...);  // Push all arguments to the Lua stack
		registerNativeFunction(name, func, sizeof...(args));
	}

	/**
	 * @brief Register a C++ callable to be callable from Lua under `name`.
	 *
	 * \throws std::logic_error if called on a borrowed State (one constructed
	 *         from an existing lua_State*). The closure captures `this` and the
	 *         callback list is instance-local, so only the State that owns the
	 *         lua_State may register methods.
	 */
	void registerMethod(const char* name, Method method);

	/**
	 * @brief Register a debug hook
	 * The debug hook is called whenever a certain event occurs in the lua virtual machine.
	 * The hook is called with the State instance and the debug information.
	 * The mask defines for which events the hook should be called. Use one ore more of the
	 * following constants to define the mask:
	 * - MaskCall: Call event
	 * - MaskReturn: Return event
	 * - MaskLine: Line event
	 * - MaskCount: Count event
	 * @param hook The function to call
	 * @param mask The mask of events for which the hook should be called
	 * @param count The number of instructions between each call of the hook
	 * \throws std::logic_error if called on a borrowed State (one constructed
	 *         from an existing lua_State*). The hook entry is keyed by the
	 *         lua_State in a process-wide table that only an owning State's
	 *         destructor cleans up, so only the owner may register hooks.
	*/
	void registerDebugHook(DebugHook hook, int mask, int count = 0);

	/**
	 * @brief Override an existing lua function with the given native function to be callable from Lua
	*/
	void overrideLuaFunction(const char* name, NativeFunction func);

	/**
	 * @brief Load a script into the registry
	 * @param key  Registry key under which the loaded chunk is stored
	 * @param code The source code of the script
	 *
	 * Reports a Load-category error via the configured handler on failure.
	*/
	template <typename T>
	LuaError::Status loadScript(T key, const char* code) {
		return reportStatus(LuaError::Category::Load, m_registry.loadScript<T>(key, code));
	}

	template <typename T>
	LuaError::Status loadScript(T key, const std::string& code) { return loadScript<T>(key, code.c_str()); }

	/**
	 * @brief Load a Lua script from a file into the registry
	 *
	 * Resolved by the overload set when @p path is a Lua::File (alias for
	 * std::filesystem::path). Uses luaL_loadfile so Lua tracebacks reference
	 * the actual file path instead of "[string \"...\"]".
	 *
	 * @param key  Registry key under which the loaded chunk is stored
	 * @param path Filesystem path to the .lua source file
	 *
	 * Reports a Load-category error via the configured handler and returns
	 * the status. Status::Ok on success.
	 */
	template <typename T>
	LuaError::Status loadScript(T key, const File& path) {
		return reportStatus(LuaError::Category::Load, m_registry.loadScriptFromFile<T>(key, path));
	}

	/**
	 * @brief Execute a script previously loaded into the registry.
	 *
	 * Reports a Runtime-category error via the configured handler on failure
	 * (including a missing or non-function registry key) and returns the
	 * status. Status::Ok on success.
	 */
	template <typename T>
	LuaError::Status executeScript(T key) {
		const auto rc = m_registry.getScript(key);
		if (rc != LuaError::Status::Ok) {
			// getScript reports two distinct conditions and we must preserve
			// the distinction:
			//  - RuntimeError: the key resolved but the stored value isn't a
			//                  function (or nothing is stored under it).
			//  - InvalidKey:   the Generic key carries an unsupported type.
			// Both are luacpp-side detections, not real pcall failures.
			const auto status = (rc == LuaError::Status::RuntimeError)
			    ? LuaError::Status::RegistryKeyNotFound
			    : rc;
			reportError(LuaError{
			    LuaError::Category::Runtime, status,
			    status == LuaError::Status::InvalidKey
			        ? "executeScript: registry key has an unsupported type"
			        : "executeScript: registry key is missing or not a function"});
			return status;
		}
		return reportStatus(LuaError::Category::Runtime, callFunction(0, 0));
	}

	/**
	 * @brief Load and execute a script. Reports Load or Runtime errors via
	 *        the configured handler and returns the status. Status::Ok on success.
	*/
	LuaError::Status loadAndExecuteScript(const char* code);

	LuaError::Status loadAndExecuteScript(const std::string& code) { return loadAndExecuteScript(code.c_str()); }

	/**
	 * @brief Load and execute a Lua script from a file
	 *
	 * Resolved by the overload set when the argument is a Lua::File (alias
	 * for std::filesystem::path). Reports Load (including FileError) or
	 * Runtime errors via the configured handler and returns the status.
	 *
	 * @param path Filesystem path to the .lua source file
	 */
	LuaError::Status loadAndExecuteScript(const File& path);

	/**
	 * @brief Call a Lua function by name. Reports a Runtime-category error
	 *        if the name is not a function or if the call fails, and returns
	 *        the status. Status::Ok on success.
	 */
	template <int NumRet = 0, typename... Args>
	LuaError::Status executeFunction(std::string_view name, Args... args) {
		if (!loadFunction(name.data())) {
			// loadFunction already popped the non-function value on failure.
			reportError(LuaError{
			    LuaError::Category::Runtime, LuaError::Status::FunctionNotFound,
			    std::string("executeFunction: '") + std::string(name) + "' is not a function"});
			return LuaError::Status::FunctionNotFound;
		}
		(pushToStack(args), ...);
		return reportStatus(LuaError::Category::Runtime, callFunction(sizeof...(args), NumRet));
	}

	template <int NumRet = 0, typename T>
	LuaError::Status executeFunctionWithArgsArray(std::string_view name, const T* args, size_t numArgs) {
		if (!loadFunction(name.data())) {
			// loadFunction already popped the non-function value on failure.
			reportError(LuaError{
			    LuaError::Category::Runtime, LuaError::Status::FunctionNotFound,
			    std::string("executeFunctionWithArgsArray: '") + std::string(name) + "' is not a function"});
			return LuaError::Status::FunctionNotFound;
		}
		for (size_t i = 0; i < numArgs; ++i) {
			pushToStack<T>(args[i]);
		}
		return reportStatus(LuaError::Category::Runtime, callFunction(static_cast<int>(numArgs), NumRet));
	}

	/**
	 * @brief Call a Lua function expecting a single return value. Returns
	 *        nullopt when the call failed (in which case the handler was
	 *        also invoked) or when the returned value's type does not match T.
	 */
	template <typename T, typename... Args>
	[[nodiscard]] std::optional<T> executeFunctionReturning(std::string_view name, Args... args) {
		if (executeFunction<1>(name, args...) != LuaError::Status::Ok) {
			return std::nullopt;
		}
		return popTypedReturn<T>();
	}

	template <typename T>
	[[nodiscard]] std::optional<T> executeFunctionWithArgsArrayReturning(std::string_view name, const T* args, size_t numArgs) {
		if (executeFunctionWithArgsArray<1>(name, args, numArgs) != LuaError::Status::Ok) {
			return std::nullopt;
		}
		return popTypedReturn<T>();
	}

#if LUACPP_HAS_SPAN
	/**
	 * @brief std::span overload of executeFunctionWithArgsArray (C++20+).
	 *
	 * Additive convenience that supersedes the (pointer, length) signature: the
	 * caller passes a single std::span instead of splitting a range into a
	 * pointer and a length. CTAD makes building one from any contiguous
	 * container a one-liner — e.g. executeFunctionWithArgsArray(name,
	 * std::span(vec)). Element type is deduced; a span of const elements is
	 * accepted (the values are only read and pushed).
	 *
	 * Only declared when the consumer compiles with C++20 and <span> support
	 * (LUACPP_HAS_SPAN). The C++17 (pointer, length) overload above is always
	 * present and unaffected. Forced off by defining LUACPP_NO_MODERN.
	 */
	template <int NumRet = 0, typename T, std::size_t Extent>
	LuaError::Status executeFunctionWithArgsArray(std::string_view name, std::span<T, Extent> args) {
		return executeFunctionWithArgsArray<NumRet, std::remove_const_t<T>>(
		    name, args.data(), args.size());
	}

	/// std::span overload of executeFunctionWithArgsArrayReturning (C++20+).
	/// See the span overload of executeFunctionWithArgsArray for the rationale.
	/// Templated on Extent so both fixed-extent (e.g. from std::array) and
	/// dynamic-extent spans bind without an explicit conversion.
	template <typename T, std::size_t Extent>
	[[nodiscard]] std::optional<std::remove_const_t<T>>
	executeFunctionWithArgsArrayReturning(std::string_view name, std::span<T, Extent> args) {
		return executeFunctionWithArgsArrayReturning<std::remove_const_t<T>>(
		    name, args.data(), args.size());
	}
#endif // LUACPP_HAS_SPAN

	template <typename... Args>
	int setReturnValue(Args... args) {
		(pushToStack(args), ...);  // Push all arguments to the Lua stack
		return sizeof...(args);
	}

	/**
	 * @brief Create a new userdata object managed by lua
	 * The userdata object is allocated on the lua stack and can be accessed by the script. It is automatically freed when it
	 * is not used anymore (garbage collection).
	 * 
	 * @tparam T The type of the userdata object
	 * @tparam Args Constructor argument types
	 * @return The userdata object
	*/
	template <class T, typename... Args>
	T* createUserData(Args&&... args) {
		void* userData = Basics::allocateUserData(m_state, sizeof(T)); //allocate the userdata on the lua stack
		return new (userData) T(std::forward<Args>(args)...); //call constructor via placement new
	}

	/**
	 * @brief Get the type of a value on the stack
	*/
	Type getType(int index = -1) const;

	/**
	 * @brief Access a global scope variable from the script and push it to the stack
	 * @param name The name of the variable
	 * @return The type of the variable
	*/
	Type pushGlobalToStack(const char* name);

	/**
	 * @brief Writes the stack value into the global variable given by name
	 * @param name The name of the variable to write into
	*/
	void setGlobalFromStack(const char* name);

	/**
	 * @brief Get the number of values on the stack
	*/
	int getStackSize() const;
	
	/**
	 * @brief Pop a number of values from the stack
	*/
	void popStack(int numValues) { return Basics::popStack(m_state, numValues); }

	/**
	 * @brief Get a value from the stack
	 * @param index The index of the value on the stack
	 * @return The value
	*/
	template <typename T>
	T getStackValue(int index) const { return Stack<T>::get(m_state, index); }
	template <typename T>
	T getArgument(int index) const { return Stack<T>::get(m_state, index); }

	/**
	 * @brief Get a value from the upvalue list
	 * @param index The index of the value in the upvalue list
	 * @return The value
	*/
	template <typename T>
	T getUpValue(int index) const { return getStackValue<T>(Basics::calcUpValueIndex(index)); }

	/**
	 * @brief check if the value on the stack is of the given type
	 * @param t The type to check against
	 * @param index The index of the value on the stack
	 * @return true if the value is of the given type, false otherwise
	*/
	bool isOfType(Type t, int index) const { return Basics::isOfType(m_state, t, index); }


	/**
	 * \brief work on the table with the given name
	 * This method pushes the table with the given name from the global scope onto the stack and calls the given function.
	*/
	void withTableDo(std::string_view tableName, TableFunction workOnTable, bool createIfMissing);

	/**
	 * \brief work on the table stored on the given stack index
	 * This method reads the table on the given stack position and calls the given function.
	*/
	void withTableDo(int index, TableFunction workOnTable);

	/**
	 * \brief create a new table with the given name
	 * This method pushes a new table onto the stack and calls the given function.
	 * If name is not null, the table will be added to the global scope with the given name.
	 * If name is null, the table will be left on the stack.
	 * @param name The name of the table in the global scope (if null the table will be left on the stack)
	 * @param workOnTable The function to call
	*/
	void createTable(const char* name, TableFunction workOnTable);

	/**
	 * \brief create a new metatable with the given name
	 * This method pushes a new table onto the stack and calls the given function.
	 * @param name The name of the metatable (may not be null)
	 * @param workOnTable The function to call
	*/
	void createMetaTable(const char* name, TableFunction workOnTable);

	/**
	 * \brief assign a metatable to the table on top of the stack
	 * This method assumes the table to work on is on top of the stack.
	 * @param name The name of the metatable
	 * @return true if the metatable was found and assigned, false otherwise
	*/
	bool assignMetaTable(const char* name);

	/**
	 * @brief Push a value to the stack
	 * @param value The value to push
	*/
	template <typename T>
	void pushToStack(T value) { return Stack<T>::push(m_state, value); }

	/**
	 * @brief Push a string by reference, without copying its bytes.
	 *
	 * The caller MUST guarantee that the buffer behind @p s outlives every Lua
	 * reference to it — i.e. either until lua_close, or until transferOwnership
	 * has handed the holding container to Lua's GC.
	 *
	 * @note Available since Lua 5.5 (uses lua_pushexternalstring).
	 * @note Mutating @p s while Lua holds a reference is undefined behavior;
	 *       Lua relies on string immutability for hash caching and interning.
	 */
	void pushExternalString(const std::string& s);
	void pushExternalString(std::string&&) = delete;

	/**
	 * @brief Anchor a C++ object in this state's registry, transferring its
	 *        lifetime to the Lua GC.
	 *
	 * The object is move-constructed onto the heap and held by a Lua userdata
	 * whose __gc deletes it. The userdata is anchored in the registry so the
	 * object survives until lua_close (or until you explicitly release the
	 * registry reference, which this API does not currently expose).
	 *
	 * Intended use: keep a container alive whose elements were previously
	 * published via pushExternalString. After this call the original is
	 * moved-from; do not access it.
	 *
	 * @note @p T must be node-address-stable under move (std::map,
	 *       std::unordered_map, std::list, std::deque, or any
	 *       container that does not relocate its elements on move).
	 *       std::string is supported as a special case: the SBO/heap branch
	 *       in pushExternalString and here keeps the contract consistent
	 *       (SBO strings are copied at push time and need no anchoring;
	 *       heap strings are pointer-swapped into a registry-anchored holder).
	 */
	template <typename T>
	void transferOwnership(T&& obj) {
		using Owned = std::decay_t<T>;
		if constexpr (std::is_same_v<Owned, std::string>) {
			transferStringOwnership(std::forward<T>(obj));
		} else {
			auto* raw = new Owned(std::forward<T>(obj));
			anchorOwned(raw, &State::deleteTyped<Owned>);
		}
	}


	/**
	 * @brief reads the complete stack
	 * This method returns a simple list of all values on the stack. The values
	 * are not removed from the stack and provided in the order they are on the stack.
	 * 
	 * @return A list of all values on the stack (given as Generic objects)
	*/
	std::vector<Generic> getStack() {
		return Debug::readStack(m_state);
	}

	/**
	 * \brief Install the passive observer for LuaErrors ("where to record").
	 *
	 * Default-constructed States carry a StreamLogger writing to std::cerr.
	 * Pass nullptr to silence logging entirely.
	 *
	 * The logger (and handler) are part of a per-VM ErrorPolicy: a State
	 * passed to a debug-hook callback shares the owning State's policy, so
	 * configuring it here is observed from inside hooks too.
	 *
	 * \see ErrorHandling.hpp
	 */
	void setLogger(std::unique_ptr<ErrorLogger> logger);

	/// Convenience: construct a Logger in place, install it, return a
	/// reference for later inspection (typically MemoryLogger).
	template <typename LoggerT, typename... Args>
	LoggerT& installLogger(Args&&... args) {
		auto logger = std::make_unique<LoggerT>(std::forward<Args>(args)...);
		LoggerT* ptr = logger.get();
		setLogger(std::move(logger));
		return *ptr;
	}

	/**
	 * \brief Install the active reaction for LuaErrors ("what to do about it").
	 *
	 * Default = nullptr (no reaction; logger still runs). Set ThrowHandler
	 * to escalate errors as C++ exceptions, or a CallbackHandler for custom
	 * flow control. The logger runs before the handler, so a throwing handler
	 * does not erase the log record.
	 *
	 * \see ErrorHandling.hpp
	 */
	void setErrorHandler(std::unique_ptr<ErrorHandler> handler);

	/// Convenience: construct a Handler in place, install it, return a
	/// reference for later inspection.
	template <typename HandlerT, typename... Args>
	HandlerT& installErrorHandler(Args&&... args) {
		auto handler = std::make_unique<HandlerT>(std::forward<Args>(args)...);
		HandlerT* ptr = handler.get();
		setErrorHandler(std::move(handler));
		return *ptr;
	}

	/**
	 * \brief Install the sink for Lua's warning system (lua_setwarnf).
	 *
	 * Default is no luacpp logger — Lua's own warnfon prints to stderr.
	 * Installing a non-null logger routes warnings here instead; passing
	 * nullptr disables the warning system. See WarningHandling.hpp for the
	 * full opt-in / control-directive story.
	 *
	 * \throws std::logic_error on a borrowed State (the warning slot is
	 *         VM-global and belongs to the lua_State's creator).
	 */
	void setWarningLogger(std::unique_ptr<WarningLogger> logger);

	/// Convenience: construct a WarningLogger in place, install it, return
	/// a reference for later inspection (typically MemoryWarningLogger).
	/// Throws std::logic_error on a borrowed State, like setWarningLogger.
	template <typename LoggerT, typename... Args>
	LoggerT& installWarningLogger(Args&&... args) {
		auto logger = std::make_unique<LoggerT>(std::forward<Args>(args)...);
		LoggerT* ptr = logger.get();
		setWarningLogger(std::move(logger));
		return *ptr;
	}

	/**
	 * \brief returns the internal lua state
	*/

	lua_State* getState() const { return m_state; }

	/**
	 * \brief Bind a C++ constructor for type T as a callable Lua function
	 * \tparam T The type to bind a constructor for
	 * \tparam Args The argument types for the constructor
	 * \param name The name of the constructor function in Lua
	 */
	template <typename T, typename... Args>
	void bindConstructor(const char* name) {
		Bind::constructor<T, Args...>(*this, name);
	}

	/**
	 * \brief Bind a C++ member function as a Lua method on T's metatable
	 * \tparam T The class whose metatable receives the method
	 * \tparam Method Non-type template parameter: pointer-to-member-function
	 * \param name Name of the method in Lua
	 *
	 * Prerequisite: Metatable<T>::registerMetatable(*this) must have been called.
	 */
	template <typename T, auto Method>
	void bindMethod(const char* name) {
		Bind::method<T, Method>(*this, name);
	}

	/**
	 * \brief Bind a C++ data member as a Lua property on T's metatable
	 * \tparam T The class whose metatable receives the property
	 * \tparam Field Non-type template parameter: pointer-to-member-data
	 * \param name Name of the property in Lua
	 *
	 * Prerequisite: Metatable<T>::registerMetatable(*this) must have been called.
	 */
	template <typename T, auto Field>
	void bindProperty(const char* name) {
		Bind::property<T, Field>(*this, name);
	}

	/**
	 * \brief Attach a value as a static field on a constructor table.
	 * \param tableName Name of the constructor table (must already exist)
	 * \param fieldName Field key
	 * \param value Value (primitive, string, or user type with Metatable)
	 */
	template <typename V>
	void bindStaticField(const char* tableName, const char* fieldName, V value) {
		Bind::staticField(*this, tableName, fieldName, std::forward<V>(value));
	}

	/**
	 * \brief Attach a free function as a static method on a constructor table.
	 * \tparam Fn Non-type template parameter: pointer-to-function
	 */
	template <auto Fn>
	void bindStaticFunction(const char* tableName, const char* funcName) {
		Bind::staticFunction<Fn>(*this, tableName, funcName);
	}

	/**
	 * \brief Get the registry for direct access
	 */
	Registry& getRegistry() { return m_registry; }

private:
	constexpr static const char* const HandleName = "StateHandle";
	constexpr static const char* const GlobalScope = "_G";

	static int dispatchMethod(lua_State* state);

	void anchorOwned(void* ptr, void (*deleter)(void*));
	void transferStringOwnership(std::string s);

	// Pops the error message from the top of the Lua stack (if any) and
	// returns it packaged as a LuaError. Made explicit so the stack effect
	// is visible at the call site: pair it with reportError(LuaError) for
	// the Lua-status path.
	LuaError popErrorFromStack(LuaError::Category category, LuaError::Status status);

	// Fans a LuaError out to the configured logger and handler. The logger
	// runs first so a throwing handler does not erase the log. No Lua-stack
	// side effects — use popErrorFromStack() to construct an error from the
	// stack, or construct a LuaError directly for synthetic failures.
	void reportError(LuaError err);

	// Choke-point for the "Lua API returned a status code" pattern. On Ok
	// this is a no-op that returns Ok; on any error status it pops the error
	// from the stack and dispatches to logger/handler. Both overloads exist
	// so callers that already hold a Status don't round-trip through int.
	LuaError::Status reportStatus(LuaError::Category category, int rawStatus);
	LuaError::Status reportStatus(LuaError::Category category, LuaError::Status status);

	// Throws std::logic_error if this State does not own its lua_State.
	// `api` is the method name embedded in the message; each call site
	// documents the specific owner-only reason.
	void requireOwnedState(const char* api) const;

	template <typename U>
	static void deleteTyped(void* p) noexcept { delete static_cast<U*>(p); }

	// Shared tail for the *Returning overloads: read the top stack value via
	// Stack<T>::tryGet (which handles both class-pointer userdata and value
	// types correctly), then always pop the one return slot the caller's
	// pcall(..., 1) reserved. The earlier `getTypeFor<T>() == getType(-1)`
	// gate was broken for class pointers — every T* mapped to LightUserData
	// while bound objects live as full userdata, so the optional was always
	// nullopt for bound returns.
	template <typename T>
	std::optional<T> popTypedReturn() {
		std::optional<T> result = Stack<T>::tryGet(m_state, -1);
		popStack(1);
		return result;
	}

	/**
	 * @brief loads a function from the global scope onto the stack
	 * @param funcName The name of the function
	 * @return true if the function was found and loaded, false otherwise
	*/
	bool loadFunction(const char* funcName);

	/**
	 * @brief calls a function that is on the stack
	 * @param numArgs The number of arguments that are on the stack
	 * @param numResults The number of results that are expected
	 * @return The status of the lua virtual machine
	*/
	int callFunction(int numArgs, int numResults);

	// Alias so existing call-sites read naturally. The struct definition
	// lives in detail::StateRegistry, which owns the VM-keyed registry.
	using StateContext = detail::StateContext;

	// Declaration order matters: m_state is the cached pointer, m_context
	// is the intrusive ref into the registry, m_registry needs m_state.
	lua_State*          m_state;
	StateContext*       m_context;
	Registry            m_registry;
	std::vector<Method> m_callbacks;

	// True iff this State's ctor was the one that inserted the context
	// entry. Used by APIs whose per-instance state can't be transferred —
	// registerMethod (`this` upvalue capture), registerDebugHook, and
	// setWarningLogger (trampoline ud=this).
	bool                m_isMain;

	// Warning sink + assembly state for multi-piece warn() messages.
	// m_warningsEnabled tracks the @on/@off toggle. m_warningInProgress
	// marks the span from the first piece through the terminal piece, so we
	// can detect "first piece" without using m_warningBuffer.empty() as a
	// proxy (which would misclassify an empty first piece followed by
	// another piece). m_warningIsSinglePiece records the first piece's
	// tocont so handleWarning can apply Lua's "control only if single-piece"
	// rule at the terminal call.
	std::unique_ptr<WarningLogger> m_warningLogger;
	std::string                    m_warningBuffer;
	bool                           m_warningsEnabled = false;
	bool                           m_warningInProgress = false;
	bool                           m_warningIsSinglePiece = false;

	// lua_WarnFunction trampoline; forwards to handleWarning via ud = this.
	static void warnFunctionTrampoline(void* ud, const char* msg, int tocont);
	void handleWarning(const char* msg, int tocont);
};

} // namespace Lua

// Template implementations for Bind::method, Bind::property, etc.
// Included here — after the State class is fully defined — so BindImpl.inl
// can freely use State's interface. This also lets users include Bind.hpp
// directly without depending on include order.
#include "detail/BindImpl.inl"

#endif // LUACPP_STATE_HPP