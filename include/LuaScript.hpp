#ifndef SCRIPTING_LUASCRIPT_HPP
#define SCRIPTING_LUASCRIPT_HPP

#include "ScriptSource.hpp"
#include "LuaTable.hpp"

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

struct lua_State;

namespace Promess {
namespace UFM6 {

constexpr uint32_t bit(uint32_t n) { return 1 << n; }

class LuaScript {
public:
	typedef int (*NativeFunction)(lua_State*);
	typedef std::function<void(LuaTable&)> TableFunction;
	typedef uint32_t Library;

	constexpr static const Library LibNone = 0;
	constexpr static const Library LibBase = bit(0);
	constexpr static const Library LibPackage = bit(1);
	constexpr static const Library LibCoroutine = bit(2);
	constexpr static const Library LibTable = bit(3);
	constexpr static const Library LibIO = bit(4);
	constexpr static const Library LibOS = bit(5);
	constexpr static const Library LibString = bit(6);
	constexpr static const Library LibMath = bit(7);
	constexpr static const Library LibUTF8 = bit(8);
	constexpr static const Library LibDebug = bit(9);
	constexpr static const Library LibAll = 0xFFFFFFFF;
	constexpr static size_t LibraryCount = 10;

	struct MetaTable {
		constexpr static const char* const Addition = "__add"; ///< addition operator (+)
		constexpr static const char* const Substraction = "__sub"; ///< subtraction operator (-)
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

	enum class Type : int {
		None = -1,
		Nil = 0,
		Boolean = 1,
		LightUserData = 2,
		Number = 3,
		String = 4,
		Table = 5,
		Function = 6,
		UserData = 7,
		Thread = 8
	};

	LuaScript(Library libraries = LibAll);
	LuaScript(lua_State* state);
	LuaScript(const LuaScript&) = delete;
	LuaScript(LuaScript&& mv);

	~LuaScript();

	void openLibrary(Library library);

	/**
	 * @brief Register a native function to be callable from Lua
	*/
	int registerNativeFunction(const char* name, NativeFunction func, int numUpValues = 0);

	/**
	 * 
	*/
	template <typename... Args>
	int registerNativeFunctionWithUpvalues(const char* name, NativeFunction func, Args... args) {
		(pushToStack(args), ...);  // Push all arguments to the Lua stack
		int status = registerNativeFunction(name, func, sizeof...(args));
		if (status != 0) {
			popStack(sizeof...(args)); //pop the upvalues from the stack (on success this is done by lua)
		}
		return status;
	
	}

	/**
	 * @brief Override an existing lua function with the given native function to be callable from Lua
	*/
	int overrideLuaFunction(const char* name, NativeFunction func);

	/**
	 * @brief Load a script into the global scope
	 * The script is than available as a function with the name provided by the source (e.g. the file name)
	 * You can than execute the script by calling the function with the same name.
	 * This way you can load multiple scripts into the same lua state but this also comes with the risk of name collisions.
	 * This scripts will share the same variables and functions.
	 * This is also useful if you want to run a script multiple times.
	 * @param source The source of the script
	*/
	int loadScriptIntoGlobal(const ScriptSource& source);

	/**
	 * @brief Load and execute a script
	 * This will load the script and executes it immediately. The script will not be loaded into the global scope. So it is
	 * not available as a function to call a second time.
	*/
	int loadAndExecuteScript(const ScriptSource& source);

	template <int NumRet = 0, typename... Args>
	int executeFunction(std::string_view name, Args... args) {
		int status = 0;
		if (loadFunction(name.data())) {
			(pushToStack(args), ...);  // Push all arguments to the Lua stack
			status = callFunction(sizeof...(args), NumRet);  // Call the function with the number of arguments
		}
		return status;
	}

	template <int NumRet = 0, typename T>
	int executeFunctionWithArgsArray(std::string_view name, T* args, size_t numArgs) {
		int status = 0;
		if (loadFunction(name.data())) {
			for (size_t i = 0; i < numArgs; ++i) {
				pushToStack<T>(args[i]);
			}
			status = callFunction(0, NumRet);  // Call the function with the number of arguments
		}
		return status;
	}

	template <typename T, typename... Args>
	int executeFunctionAndReadReturnVal(T& result, std::string_view name, Args... args) {
		int status = executeFunction<1>(name, args...);
		if (status == 0) {
			result = getStackValue<T>(-1);
			popStack(1);
			return 0;
		}
		return status;
	}

	template <typename T>
	int executeFunctionWithArgsArrayAndReadReturnVal(T& result, std::string_view name, T* args, size_t numArgs) {
		int status = executeFunctionWithArgsArray<1>(name, args, numArgs);
		if (status == 0) {
			result = getStackValue<T>(-1);
			popStack(1);
			return 0;
		}
		return status;
	}


	template <typename... Args>
	int setReturnValue(Args... args) {
		(pushToStack(args), ...);  // Push all arguments to the Lua stack
		return sizeof...(args);
	}

	/**
	 * @brief Create a new userdata object managed by lua
	 * 
	 * The userdata object is allocated on the lua stack and can be accessed by the script. It is automatically freed when it
	 * is not used anymore (garbage collection).
	 * 
	 * @tparam T The type of the userdata object
	 * @return The userdata object
	*/
	template <class T>
	T* createUserData() {
		void* userData = allocateUserData(m_state, sizeof(T)); //allocate the userdata on the lua stack
		return new (userData) T(); //call constructor via placement new
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
	 * @brief Get the number of values on the stack
	*/
	int getStackSize() const;
	
	/**
	 * @brief Pop a number of values from the stack
	*/
	void popStack(int numValues);

	/**
	 * @brief Get a value from the stack
	 * @param index The index of the value on the stack
	 * @return The value
	*/
	template <typename T>
	T getStackValue(int index) const { return getStackValue<T>(m_state, index); }
	template <typename T>
	T getArgument(int index) const { return getStackValue<T>(m_state, index); }

	/**
	 * @brief Get a value from the upvalue list
	 * @param index The index of the value in the upvalue list
	 * @return The value
	*/
	template <typename T>
	T getUpValue(int index) const { return getStackValue<T>(m_state, calcUpValueIndex(index)); }

	/**
	 * @brief check if the value on the stack is of the given type
	 * @param t The type to check against
	 * @param index The index of the value on the stack
	 * @return true if the value is of the given type, false otherwise
	*/
	bool isOfType(Type t, int index) const;


	/**
	 * \brief work on the table with the given name
	 * This method pushes the table with the given name from the global scope onto the stack and calls the given function.
	*/
	void withTableDo(std::string_view tableName, TableFunction workOnTable);

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
	void pushToStack(T value) { return pushToStack(m_state, value); }


	/**
	 * \brief Get the list of errors that occured during script execution
	*/
	const std::vector<std::string>& getErrorList() const { return m_errorList; }

	/**
	 * \brief Clear the list of errors that occured during script execution
	*/
	void clearErrorList() { m_errorList.clear(); }

	/**
	 * \brief Check if the given type matches the template type
	*/
	template <typename T>
	static bool doesTypeMatch(Type type) {
		switch (type) {
			case Type::Nil:	return std::is_same_v<T, std::nullptr_t>;
			case Type::Boolean:	return std::is_same_v<T, bool>;
			case Type::LightUserData: return std::is_pointer_v<T>;
			case Type::Number: return std::is_floating_point_v<T> || std::is_integral_v<T>;
			case Type::String: return std::is_same_v<T, const char*> || std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>;
			case Type::Table:
			case Type::Function:
			case Type::UserData:
			case Type::Thread:
			default:
				return false;
		}
		return false;
	}

private:
	constexpr static const char* const HandleName = "LuaScriptHandle";
	constexpr static const char* const GlobalScope = "_G";
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

	lua_State* m_state; ///< instance of the lua virtual machine
	bool m_externalState; ///< true if the state was provided by the user, false if it was created by this class
	std::vector<std::string> m_errorList; ///< list of errors that occured during script execution


	static void* allocateUserData(lua_State* state, size_t size, int userValues = 0);

	template <typename T>
	static T getStackValue(lua_State* state, int index) {
		if constexpr (std::is_pointer_v<T>) {
			return static_cast<T>(asUserData(state, index));
		} else if constexpr (std::is_same_v<T, bool>) {
			return asBoolean(state, index);
		} else if constexpr (std::is_floating_point_v<T>) {
			return asNumber(state, index);
		} else if constexpr (std::is_integral_v<T>) {
			return asInteger(state, index);
		} else if constexpr (std::is_same_v<T, const char*>) {
			return asString(state, index);
		} else if constexpr (std::is_same_v<T, std::string_view>) {
			size_t len;
			const char* str = asString(state, index, &len);
			return std::string_view(str, len);
		} else if constexpr (std::is_same_v<T, std::string>) {
			size_t len;
			const char* str = asString(state, index, &len);
			return std::string(str, len);
		} else {
			static_assert(sizeof(T) != sizeof(T), "Unsupported type");
		}
	}

	template <typename T>
	static void pushToStack(lua_State* state, T value) {
		if constexpr (std::is_same_v<T, bool>) {
			pushBoolean(state, value);
		} else if constexpr (std::is_floating_point_v<T>) {
			pushNumber(state, value);
		} else if constexpr (std::is_integral_v<T>) {
			pushInteger(state, value);
		} else if constexpr (std::is_same_v<T, const char*>) {
			pushString(state, value);
		} else if constexpr (std::is_same_v<T, std::string_view>) {
			pushString(state, value.data(), value.length());
		} else if constexpr (std::is_same_v<T, std::string>) {
			pushString(state, value.c_str(), value.length());
		} else if constexpr (std::is_same_v<T, NativeFunction>) {
			pushCFunction(state, value);
		} else if constexpr (std::is_pointer_v<T>) {
			pushLightUserData(state, value);
		} else {
			static_assert(sizeof(T) != sizeof(T), "Unsupported type");
		}
	}

	static int calcUpValueIndex(int index);

	static void* asUserData(lua_State* state, int index);
	static bool asBoolean(lua_State* state, int index);
	static double asNumber(lua_State* state, int index);
	static int64_t asInteger(lua_State* state, int index);
	static const char* asString(lua_State* state, int index, size_t* len = nullptr);

	static void pushBoolean(lua_State* state, bool value);
	static void pushNumber(lua_State* state, double value);
	static void pushInteger(lua_State* state, int64_t value);
	static void pushString(lua_State* state, const char* value);
	static void pushString(lua_State* state, const char* value, size_t len);
	static void pushCFunction(lua_State* state, NativeFunction value);
	static void pushLightUserData(lua_State* state, void* value);
};

template <>
inline void LuaScript::pushToStack<const std::string&>(lua_State* state, const std::string& value) {
	pushString(state, value.c_str(), value.length());
}

} // namespace UFM6
} // namespace Promess

#endif // SCRIPTING_LUASCRIPT_HPP