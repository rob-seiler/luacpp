#ifndef LUACPP_METATABLE_HPP
#define LUACPP_METATABLE_HPP

#include "State.hpp"
#include "Table.hpp"

#include <type_traits>
#include <typeinfo>
#include <utility>

namespace Lua {

template <typename, typename = void>
struct has_add_operator : std::false_type { };

template <typename T>
struct has_add_operator<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_sub_operator : std::false_type { };

template <typename T>
struct has_sub_operator<T, std::void_t<decltype(std::declval<T>() - std::declval<T>())>> : std::true_type { };

namespace detail {
	template <typename T>
	void registerDefaultMetatable(State& state) {
		state.createMetaTable(Metatable<T>::metatableName(), [](Table& mt) {
			if constexpr (has_add_operator<T>::value) {
				int (*addFunc)(lua_State*) = [](lua_State* lvm) -> int {
					State L(lvm);
					T* lhs = L.getArgument<T*>(1);
					T* rhs = L.getArgument<T*>(2);
					T result = *lhs + *rhs;
					Metatable<T>::create(L, result);
					return 1;
				};
				mt.setElement(State::MetaTable::Addition, addFunc);
			}
			if constexpr (has_sub_operator<T>::value) {
				int (*subFunc)(lua_State*) = [](lua_State* lvm) -> int {
					State L(lvm);
					T* lhs = L.getArgument<T*>(1);
					T* rhs = L.getArgument<T*>(2);
					T result = *lhs - *rhs;
					Metatable<T>::create(L, result);
					return 1;
				};
				mt.setElement(State::MetaTable::Substraction, subFunc);
			}
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
