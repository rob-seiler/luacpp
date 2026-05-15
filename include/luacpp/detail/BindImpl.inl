#ifndef LUACPP_DETAIL_BIND_IMPL_INL
#define LUACPP_DETAIL_BIND_IMPL_INL

#include "../State.hpp"
#include "../Metatable.hpp"
#include "../Table.hpp"
#include "../Basics.hpp"
#include "ArgumentExtractor.hpp"

#include <type_traits>
#include <tuple>
#include <utility>
#include <string>
#include <cstddef>

namespace Lua {
namespace detail {

// Defined in src/Bind.cpp — keeps the Lua C API out of this header.
void addMethodToMetatable(lua_State* L,
                          const char* metatableName,
                          const char* methodName,
                          Basics::NativeFunction func);

void addPropertyToMetatable(lua_State* L,
                            const char* metatableName,
                            const char* propertyName,
                            Basics::NativeFunction getter,
                            Basics::NativeFunction setter);

template <typename T, typename... Args, std::size_t... I>
T* createInUserdataImpl(State& state, int startIdx, std::index_sequence<I...>) {
	return state.createUserData<T>(ArgumentExtractor<Args>::extract(state, startIdx + static_cast<int>(I))...);
}

template <typename T, typename... Args>
T* createInUserdata(State& state, int startIdx) {
	return createInUserdataImpl<T, Args...>(state, startIdx, std::index_sequence_for<Args...>{});
}

template <typename M> struct MethodTraits;

template <typename T, typename R, typename... Args>
struct MethodTraits<R (T::*)(Args...)> {
	using ClassType  = T;
	using ReturnType = R;
	using ArgsTuple  = std::tuple<Args...>;
};

template <typename T, typename R, typename... Args>
struct MethodTraits<R (T::*)(Args...) const> {
	using ClassType  = T;
	using ReturnType = R;
	using ArgsTuple  = std::tuple<Args...>;
};

template <typename P> struct PropertyTraits;

template <typename T, typename F>
struct PropertyTraits<F T::*> {
	using ClassType = T;
	using FieldType = F;
};

template <auto Method, std::size_t... I>
int invokeMethod(lua_State* lvm, std::index_sequence<I...>) {
	using Traits    = MethodTraits<decltype(Method)>;
	using T         = typename Traits::ClassType;
	using R         = typename Traits::ReturnType;
	using ArgsTuple = typename Traits::ArgsTuple;

	State L(lvm);
	T* self = static_cast<T*>(
		Basics::checkUserData(lvm, 1, Metatable<T>::metatableName()));

	if constexpr (std::is_void_v<R>) {
		(self->*Method)(
			ArgumentExtractor<std::tuple_element_t<I, ArgsTuple>>
				::extract(L, 2 + static_cast<int>(I))...);
		return 0;
	} else {
		R result = (self->*Method)(
			ArgumentExtractor<std::tuple_element_t<I, ArgsTuple>>
				::extract(L, 2 + static_cast<int>(I))...);

		using CleanR = std::remove_cv_t<std::remove_reference_t<R>>;
		if constexpr (Basics::getTypeFor<CleanR>() == Type::None) {
			Metatable<CleanR>::create(L, std::move(result));
		} else {
			L.pushToStack(result);
		}
		return 1;
	}
}

template <auto Method>
int methodWrapper(lua_State* lvm) {
	using Traits = MethodTraits<decltype(Method)>;
	constexpr std::size_t N = std::tuple_size_v<typename Traits::ArgsTuple>;
	return invokeMethod<Method>(lvm, std::make_index_sequence<N>{});
}

template <auto Field>
int propertyGetter(lua_State* lvm) {
	using Traits    = PropertyTraits<decltype(Field)>;
	using T         = typename Traits::ClassType;
	using FieldType = typename Traits::FieldType;

	State L(lvm);
	T* self = static_cast<T*>(
		Basics::checkUserData(lvm, 1, Metatable<T>::metatableName()));

	using CleanField = std::remove_cv_t<FieldType>;
	if constexpr (Basics::getTypeFor<CleanField>() == Type::None) {
		Metatable<CleanField>::create(L, self->*Field);
	} else {
		L.pushToStack(self->*Field);
	}
	return 1;
}

template <auto Field>
int propertySetter(lua_State* lvm) {
	using Traits    = PropertyTraits<decltype(Field)>;
	using T         = typename Traits::ClassType;
	using FieldType = typename Traits::FieldType;

	State L(lvm);
	T* self = static_cast<T*>(
		Basics::checkUserData(lvm, 1, Metatable<T>::metatableName()));

	self->*Field = ArgumentExtractor<FieldType>::extract(L, 2);
	return 0;
}

} // namespace detail

template <typename T, typename... Args>
void Bind::constructor(State& state, const char* name) {
	state.createTable(name, [&state, name](Table& ctorTable) {
		std::string mtName = std::string(name) + "ConstructorMT";
		state.createMetaTable(mtName.c_str(), [](Table& mt) {
			int (*callFunc)(lua_State*) = [](lua_State* lvm) -> int {
				State L(lvm);
				detail::createInUserdata<T, Args...>(L, 2);
				L.assignMetaTable(Metatable<T>::metatableName());
				return 1;
			};
			mt.setElement(State::MetaTable::Call, callFunc);
		});
		ctorTable.assignMetaTable(mtName.c_str());
	});
}

template <typename T, auto Method>
void Bind::method(State& state, const char* name) {
	using Traits = detail::MethodTraits<decltype(Method)>;
	static_assert(std::is_same_v<typename Traits::ClassType, T>,
	              "Method must be a member function of T");
	detail::addMethodToMetatable(
		state.getState(),
		Metatable<T>::metatableName(),
		name,
		&detail::methodWrapper<Method>);
}

template <typename T, auto Field>
void Bind::property(State& state, const char* name) {
	using Traits = detail::PropertyTraits<decltype(Field)>;
	static_assert(std::is_same_v<typename Traits::ClassType, T>,
	              "Field must be a member of T");
	detail::addPropertyToMetatable(
		state.getState(),
		Metatable<T>::metatableName(),
		name,
		&detail::propertyGetter<Field>,
		&detail::propertySetter<Field>);
}

} // namespace Lua

#endif // LUACPP_DETAIL_BIND_IMPL_INL
