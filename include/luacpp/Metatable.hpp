#ifndef LUACPP_METATABLE_HPP
#define LUACPP_METATABLE_HPP

#include "State.hpp"
#include "Table.hpp"
#include "Basics.hpp"

#include <type_traits>
#include <typeinfo>
#include <utility>
#include "OperatorTraits.hpp"

namespace Lua {


namespace detail {
        // Helper to safely get typed userdata with validation
        template <typename T>
        T* checkUserData(lua_State* lvm, int index) {
                void* ud = Basics::checkUserData(lvm, index, Metatable<T>::metatableName());
                return static_cast<T*>(ud);
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
        void registerOperators(Table& mt) {
                registerAdd<T>(mt);
                registerSub<T>(mt);
                registerMul<T>(mt);
                registerDiv<T>(mt);
                registerUnaryMinus<T>(mt);
                registerEqual<T>(mt);
        }

        template <typename T>
        void registerDefaultMetatable(State& state) {
                state.createMetaTable(Metatable<T>::metatableName(), [](Table& mt) {
                        registerOperators<T>(mt);
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
