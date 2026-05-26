#ifndef LUACPP_STACK_HPP
#define LUACPP_STACK_HPP

#include "Basics.hpp"

#include <optional>
#include <string>
#include <type_traits>

struct lua_State;

namespace Lua {

// Forward declaration for Metatable
template <typename T>
struct Metatable;

namespace detail {

// Marker trait: detects whether <luacpp/Metatable.hpp> is included at the
// point where Stack<T*> is instantiated for a class type. The primary
// template is `false`; Metatable.hpp adds a partial specialization that
// matches once Metatable<T> is fully defined. The static_assert in
// Stack<T*>::get below uses this to produce a precise diagnostic instead
// of an "incomplete type" cascade when the user forgets the include.
template <typename T, typename = void>
struct metatable_visible : std::false_type {};

} // namespace detail

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
				static_assert(detail::metatable_visible<PointeeType>::value,
					"Stack<T*>::get for a class type requires <luacpp/Metatable.hpp> "
					"to be included (typically pulled in transitively via "
					"<luacpp/State.hpp>). Add the include and retry.");
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

	/**
	 * @brief Non-throwing read. Returns nullopt when the value at @p index
	 *        does not match T (wrong Lua type, or — for class pointers — a
	 *        different metatable).
	 *
	 * Use at C++/host boundaries (readVariable) where a mismatch must be a
	 * query result. get() is for callback context, where a mismatch should
	 * raise a Lua error to the enclosing pcall.
	 */
	static std::optional<T> tryGet(lua_State* state, int index) {
		if constexpr (std::is_pointer_v<T>) {
			using PointeeType = std::remove_pointer_t<T>;
			if constexpr (std::is_class_v<PointeeType>) {
				static_assert(detail::metatable_visible<PointeeType>::value,
					"Stack<T*>::tryGet for a class type requires "
					"<luacpp/Metatable.hpp> to be included (typically pulled in "
					"transitively via <luacpp/State.hpp>). Add the include and retry.");
				void* ud = Basics::testUserData(state, index, Metatable<PointeeType>::metatableName());
				if (ud == nullptr) return std::nullopt;
				return static_cast<T>(ud);
			} else {
				void* ud = Basics::asUserData(state, index);
				if (ud == nullptr) return std::nullopt;
				return static_cast<T>(ud);
			}
		} else {
			// Value types: require an exact Lua-type match before extracting,
			// so we don't lean on Lua's implicit number<->string coercion.
			if (Basics::getTypeFor<T>() != Basics::getType(state, index)) {
				return std::nullopt;
			}
			return get(state, index);
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