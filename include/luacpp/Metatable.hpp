#ifndef LUACPP_METATABLE_HPP
#define LUACPP_METATABLE_HPP

#include "State.hpp"
#include "Table.hpp"
#include "Basics.hpp"

#include <type_traits>
#include <typeinfo>
#include <utility>
#include <string>
#include "detail/OperatorTraits.hpp"

namespace Lua {


namespace detail {
/**
 * @brief Safely retrieves and validates typed userdata from the Lua stack
 * @tparam T The expected userdata type
 * @param lvm The Lua state
 * @param index Stack index of the value to check
 * @return Pointer to the validated userdata of type T
 *
 * @note This function uses luaL_checkudata internally, which:
 *       - Validates the value is userdata with matching metatable
 *       - On success: returns a valid pointer (NEVER nullptr)
 *       - On failure: throws a Lua error via longjmp (NEVER returns)
 *
 * @note Therefore, nullptr checks after calling this function are unnecessary.
 *       If the function returns, the pointer is guaranteed to be valid.
 *
 * @note Error Handling: Type mismatches automatically generate Lua errors with
 *       descriptive messages. These errors propagate through Lua's error handling
 *       system and can be caught at the script level with pcall() or will be
 *       returned as error codes from State::loadAndExecuteScript().
 *
 * Example:
 * @code
 * // In C++ callback:
 * T* obj = checkUserData<T>(lvm, 1);
 * // No null check needed - if we reach here, obj is valid
 * obj->doSomething();
 *
 * // In Lua (error handling):
 * local success, err = pcall(function()
 *     myCppFunction(wrongType) -- Will error if types don't match
 * end)
 * if not success then
 *     print("Type error: " .. err)
 * end
 * @endcode
 */
template <typename T>
T* checkUserData(lua_State* lvm, int index) {
	void* ud = Basics::checkUserData(lvm, index, Metatable<T>::metatableName());
	return static_cast<T*>(ud); // Safe cast - ud is never nullptr here
}

template <typename T>
void registerAdd(Table& mt) {
	if constexpr (has_add_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* lhs = checkUserData<T>(lvm, 1);
			T* rhs = checkUserData<T>(lvm, 2);
			T result = *lhs + *rhs;
			Metatable<T>::create(L, result);
			return 1;
		};
		mt.setElement(State::MetaTable::Addition, func);
	}
}

template <typename T>
void registerSub(Table& mt) {
	if constexpr (has_sub_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* lhs = checkUserData<T>(lvm, 1);
			T* rhs = checkUserData<T>(lvm, 2);
			T result = *lhs - *rhs;
			Metatable<T>::create(L, result);
			return 1;
		};
		mt.setElement(State::MetaTable::Substraction, func);
	}
}

template <typename T>
void registerMul(Table& mt) {
	if constexpr (has_mul_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* lhs = checkUserData<T>(lvm, 1);
			T* rhs = checkUserData<T>(lvm, 2);
			T result = *lhs * *rhs;
			Metatable<T>::create(L, result);
			return 1;
		};
		mt.setElement(State::MetaTable::Multiplication, func);
	}
}

template <typename T>
void registerDiv(Table& mt) {
	if constexpr (has_div_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* lhs = checkUserData<T>(lvm, 1);
			T* rhs = checkUserData<T>(lvm, 2);
			T result = *lhs / *rhs;
			Metatable<T>::create(L, result);
			return 1;
		};
		mt.setElement(State::MetaTable::Division, func);
	}
}

template <typename T>
void registerUnaryMinus(Table& mt) {
	if constexpr (has_unary_minus_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* obj = checkUserData<T>(lvm, 1);
			T result = -(*obj);
			Metatable<T>::create(L, result);
			return 1;
		};
		mt.setElement(State::MetaTable::UnaryMinus, func);
	}
}

template <typename T>
void registerEqual(Table& mt) {
	if constexpr (has_eq_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* lhs = checkUserData<T>(lvm, 1);
			T* rhs = checkUserData<T>(lvm, 2);
			bool result = *lhs == *rhs;
			L.pushToStack(result);
			return 1;
		};
		mt.setElement(State::MetaTable::Equal, func);
	}
}

template <typename T>
void registerToString(Table& mt) {
	if constexpr (has_to_string<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* obj = checkUserData<T>(lvm, 1);
			std::string result = obj->toString();
			L.pushToStack(result.c_str());
			return 1;
		};
		mt.setElement(State::MetaTable::Tostring, func);
	}
}

/**
 * @brief Register __gc metamethod for types with non-trivial destructors
 * @tparam T The type to register destructor for
 * @param mt The metatable to register in
 *
 * This function automatically registers a garbage collection handler that
 * properly calls the C++ destructor when Lua's garbage collector runs.
 * Only registers for types that need explicit destruction.
 */
template <typename T>
void registerGC(Table& mt) {
	// Only register __gc for types with non-trivial destructors
	if constexpr (!std::is_trivially_destructible_v<T>) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			// Get the userdata - we use asUserData here because __gc is called
			// by Lua's GC, not from user code, so type is guaranteed
			T* obj = static_cast<T*>(Basics::asUserData(lvm, 1));
			if (obj != nullptr) {
				// Explicitly call destructor (placement delete)
				obj->~T();
			}
			return 0;
		};
		mt.setElement(State::MetaTable::GC, func);
	}
}

template <typename T>
void registerOperators(Table& mt) {
	registerAdd<T>(mt);
	registerSub<T>(mt);
	registerMul<T>(mt);
	registerDiv<T>(mt);
	registerUnaryMinus<T>(mt);
	registerEqual<T>(mt);
	registerToString<T>(mt);
}

template <typename T>
void registerDefaultMetatable(State& state) {
	state.createMetaTable(Metatable<T>::metatableName(), [](Table& mt) {
		registerOperators<T>(mt);
		registerGC<T>(mt);
	});
}
} // namespace detail

/**
 * @brief Helper for binding custom C++ classes to Lua.
 *
 * Specializations can override @c metatableName() or @c registerMetatable()
 * to customize the integration. By default, common operators are registered
 * if they exist.
 */
template <typename T>
struct Metatable {
	static const char* metatableName() { return typeid(T).name(); }

	static void registerMetatable(State& state) {
		detail::registerDefaultMetatable<T>(state);
	}

	template <typename... Args>
	static T* create(State& state, Args&&... args) {
		T* obj = state.createUserData<T>(std::forward<Args>(args)...);
		state.assignMetaTable(metatableName());
		return obj;
	}

	static T* create(State& state, const T& obj) {
		T* userdata = state.createUserData<T>(obj);
		state.assignMetaTable(metatableName());
		return userdata;
	}
};

} // namespace Lua

#endif // LUACPP_METATABLE_HPP
