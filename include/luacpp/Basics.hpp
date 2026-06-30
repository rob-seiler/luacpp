#ifndef LUACPP_BASICS_HPP
#define LUACPP_BASICS_HPP

#include "Type.hpp"

#include <cstdint>
#include <string>
#include <map>

template<typename T>
struct is_map : std::false_type {};

template<typename Key, typename Value, typename... Args>
struct is_map<std::map<Key, Value, Args...>> : std::true_type {};

struct lua_State;

namespace Lua {

class Basics {
public:
	typedef int (*NativeFunction)(lua_State*);

	Basics() = delete;
	Basics(const Basics&) = delete;
	Basics(Basics&&) = delete;
	~Basics() = delete;

	template <typename T>
	constexpr static Type getTypeFor() {
		if constexpr (std::is_void_v<T> || std::is_same_v<T, std::nullptr_t>) {
			return Type::Nil;
		} else if constexpr (std::is_same_v<T, bool>) {
			return Type::Boolean;
		} else if constexpr (std::is_pointer_v<T>) {
			return Type::LightUserData;
		} else if constexpr (std::is_floating_point_v<T> || std::is_integral_v<T>) {
			return Type::Number;
		} else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, std::string_view> || std::is_same_v<T, std::string>) {
			return Type::String;
		} else if constexpr (std::is_same_v<T, NativeFunction>) {
			return Type::Function;
		} else if constexpr (is_map<T>::value) {
			return Type::Table;
		} else {
			return Type::None;
		}
	}

	/**
	 * @brief check if the value on the stack is of the given type
	 * @param t The type to check against
	 * @param index The index of the value on the stack
	 * @return true if the value is of the given type, false otherwise
	*/
	static bool isOfType(lua_State* state, Type t, int index);
	static bool isFunction(lua_State* state, int index);

	static Type getType(lua_State* state, int index);

	static void insert(lua_State* state, int index);
	static void popStack(lua_State* state, int numValues);

	/// Push the global named @p name onto the stack; returns its type.
	static Type pushGlobal(lua_State* state, const char* name);
	/// Pop the stack top into the global named @p name.
	static void setGlobal(lua_State* state, const char* name);

	static int  getStackTop(lua_State* state);
	static void setStackTop(lua_State* state, int newTop);

	static void pushNil(lua_State* state);
	static void pushBoolean(lua_State* state, bool value);
	static void pushNumber(lua_State* state, double value);
	static void pushInteger(lua_State* state, int64_t value);
	static void pushString(lua_State* state, const char* value);
	static void pushString(lua_State* state, const char* value, size_t len);

	/**
	 * @brief Deallocator invoked by Lua when an externally-managed string is collected.
	 * @note  Structurally compatible with Lua's lua_Alloc. Lua calls it with nsize==0
	 *        to signal that the buffer is no longer referenced; osize is len+1
	 *        (including the null terminator) and ptr is the original buffer.
	 */
	typedef void* (*ExternalStringDeallocator)(void* ud, void* ptr, size_t osize, size_t nsize);

	/**
	 * @brief Push a string into Lua without copying its bytes.
	 * @param value    Buffer of size len+1; the byte at value[len] MUST be '\0'.
	 * @param len      Length excluding the null terminator.
	 * @param dealloc  Called when Lua releases the reference. Pass nullptr when the
	 *                 buffer has static lifetime — Lua then never tries to free it.
	 * @param ud       Opaque pointer forwarded to dealloc as its first argument.
	 * @note The buffer must remain valid and unmodified until dealloc is invoked.
	 *       Available since Lua 5.5.
	 */
	static void pushExternalString(lua_State* state, const char* value, size_t len,
	                               ExternalStringDeallocator dealloc, void* ud);

	static void pushCFunction(lua_State* state, NativeFunction value);
	static void pushLightUserData(lua_State* state, void* value);

	static bool isInteger(lua_State* state, int index);

	/**
	 * @brief Get userdata pointer without type validation
	 * @param state The Lua state
	 * @param index Stack index
	 * @return Pointer to userdata if value is userdata, nullptr otherwise
	 * @note Does NOT validate metatable type. Use for checking if value is userdata.
	 */
	static void* asUserData(lua_State* state, int index);

	/**
	 * @brief Get userdata pointer with type validation (throws on mismatch)
	 * @param state The Lua state
	 * @param index Stack index
	 * @param tname Metatable name to check against
	 * @return Pointer to validated userdata (NEVER nullptr)
	 * @throws Lua error via longjmp if type doesn't match or value isn't userdata
	 * @note Uses luaL_checkudata which NEVER returns nullptr - it throws instead
	 * @note If this function returns, the pointer is guaranteed valid (no null check needed)
	 */
	static void* checkUserData(lua_State* state, int index, const char* tname);

	/**
	 * @brief Get userdata pointer with type validation, WITHOUT throwing.
	 * @param state The Lua state
	 * @param index Stack index
	 * @param tname Metatable name to check against
	 * @return Pointer to validated userdata, or nullptr if the value is not
	 *         userdata or carries a different metatable.
	 * @note Wraps luaL_testudata. Use at C++/host boundaries (e.g. readVariable)
	 *       where a type mismatch must be a query result, not a raised Lua
	 *       error — raising here would have no enclosing pcall and would kill
	 *       the program.
	 */
	static void* testUserData(lua_State* state, int index, const char* tname);

	static bool asBoolean(lua_State* state, int index);
	static double asNumber(lua_State* state, int index);
	static int64_t asInteger(lua_State* state, int index);
	static const char* asString(lua_State* state, int index, size_t* len = nullptr);

	static void* allocateUserData(lua_State* state, size_t size, int userValues = 0);

	static int calcUpValueIndex(int index);

	/**
	 * @brief Raise a Lua error from C++ code (never returns)
	 * @param state The Lua state
	 * @param message Error message
	 * @return int Declared as int for use in `return Basics::error(...)` patterns;
	 *             the function does not actually return (uses longjmp internally).
	 */
	static int error(lua_State* state, const char* message);
};

// Stack trait struct for push/get


} //namespace Lua

#endif //LUACPP_BASICS_HPP