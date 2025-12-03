#ifndef LUACPP_STACK_HPP
#define LUACPP_STACK_HPP

#include "Basics.hpp"

#include <string>
#include <type_traits>

struct lua_State;

namespace Lua {

// Forward declaration for Metatable
template <typename T>
struct Metatable;

template <typename T>
struct Stack {
	static void push(lua_State* state, T value) {
		if constexpr (std::is_same_v<T, bool>) {
			Basics::pushBoolean(state, value);
		} else if constexpr (std::is_floating_point_v<T>) {
			Basics::pushNumber(state, value);
		} else if constexpr (std::is_integral_v<T>) {
			Basics::pushInteger(state, value);
		} else if constexpr (std::is_same_v<T, const char*>) {
			Basics::pushString(state, value);
		} else if constexpr (std::is_same_v<T, std::string_view>) {
			Basics::pushString(state, value.data(), value.length());
		} else if constexpr (std::is_same_v<T, std::string>) {
			Basics::pushString(state, value.c_str(), value.length());
		} else if constexpr (std::is_same_v<T, Basics::NativeFunction>) {
			Basics::pushCFunction(state, value);
		} else if constexpr (std::is_pointer_v<T>) {
			Basics::pushLightUserData(state, value);
		} else {
			static_assert(sizeof(T) != sizeof(T), "Unsupported type");
		}
	}

	static T get(lua_State* state, int index) {
		if constexpr (std::is_same_v<T, const char*>) {
			return Basics::asString(state, index);
		} else if constexpr (std::is_same_v<T, std::string_view>) {
			size_t len;
			const char* str = Basics::asString(state, index, &len);
			return std::string_view(str, len);
		} else if constexpr (std::is_same_v<T, std::string>) {
			size_t len;
			const char* str = Basics::asString(state, index, &len);
			return std::string(str, len);
		} else if constexpr (std::is_pointer_v<T>) {
			using PointeeType = std::remove_pointer_t<T>;
			// For class types, use type-safe checkUserData
			if constexpr (std::is_class_v<PointeeType>) {
				const char* tname = Metatable<PointeeType>::metatableName();
				void* ud = Basics::checkUserData(state, index, tname);
				return static_cast<T>(ud);
			} else {
				// For non-class pointers (e.g., void*), use asUserData
				return static_cast<T>(Basics::asUserData(state, index));
			}
		} else if constexpr (std::is_same_v<T, bool>) {
			return Basics::asBoolean(state, index);
		} else if constexpr (std::is_floating_point_v<T>) {
			return static_cast<T>(Basics::asNumber(state, index));
		} else if constexpr (std::is_integral_v<T>) {
			return static_cast<T>(Basics::asInteger(state, index));
		} else {
			static_assert(sizeof(T) != sizeof(T), "Unsupported type");
		}
	}
};

// Specialization for const std::string&
template <>
struct Stack<const std::string&> {
	static void push(lua_State* state, const std::string& value);
};

// Specialization for Generic
class Generic;
template <>
struct Stack<Generic> {
	static void push(lua_State* state, Generic value);
};

//convenience functions for template deduction
template <typename T>
void pushToStack(lua_State* state, T value) {
	Stack<T>::push(state, value);
}

template <typename T>
T getStackValue(lua_State* state, int index) {
	return Stack<T>::get(state, index);
}

} // namespace Lua

#endif // LUACPP_STACK_HPP