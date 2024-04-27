#ifndef LUACPP_BASICS_HPP
#define LUACPP_BASICS_HPP

#include <cstdint>
#include <string>

struct lua_State;

namespace Lua {

class Basics {
public:
	typedef int (*NativeFunction)(lua_State*);

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

	Basics() = delete;
	Basics(const Basics&) = delete;
	Basics(Basics&&) = delete;
	~Basics() = delete;

	/**
	 * @brief check if the value on the stack is of the given type
	 * @param t The type to check against
	 * @param index The index of the value on the stack
	 * @return true if the value is of the given type, false otherwise
	*/
	static bool isOfType(lua_State* state, Type t, int index);

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

	template <typename T>
	static T getStackValue(lua_State* state, int index) {
		if constexpr (std::is_pointer_v<T>) {
			return static_cast<T>(asUserData(state, index));
		} else if constexpr (std::is_same_v<T, bool>) {
			return asBoolean(state, index);
		} else if constexpr (std::is_floating_point_v<T>) {
			return static_cast<T>(asNumber(state, index));
		} else if constexpr (std::is_integral_v<T>) {
			return static_cast<T>(asInteger(state, index));
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

	static void popStack(lua_State* state, int numValues);

	static void pushBoolean(lua_State* state, bool value);
	static void pushNumber(lua_State* state, double value);
	static void pushInteger(lua_State* state, int64_t value);
	static void pushString(lua_State* state, const char* value);
	static void pushString(lua_State* state, const char* value, size_t len);
	static void pushCFunction(lua_State* state, NativeFunction value);
	static void pushLightUserData(lua_State* state, void* value);

	static void* asUserData(lua_State* state, int index);
	static bool asBoolean(lua_State* state, int index);
	static double asNumber(lua_State* state, int index);
	static int64_t asInteger(lua_State* state, int index);
	static const char* asString(lua_State* state, int index, size_t* len = nullptr);

	static void* allocateUserData(lua_State* state, size_t size, int userValues = 0);

	static int calcUpValueIndex(int index);
};

template <>
inline void Basics::pushToStack<const std::string&>(lua_State* state, const std::string& value) {
	pushString(state, value.c_str(), value.length());
}

} //namespace Lua

#endif //LUACPP_BASICS_HPP