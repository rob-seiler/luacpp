#ifndef LUACPP_BIND_HPP
#define LUACPP_BIND_HPP

// Public header for class binding. Include this in translation units that
// register C++ classes with Lua (state.binding.* / Bind::*). It pulls in a
// complete State first, then defines the Bind templates against it — so the
// definitions live in an ordinary header included *after* State, instead of
// being appended to <luacpp/State.hpp> as an .inl. State.hpp itself stays free
// of binding implementation and does not need to be included for class binding
// to work (this header includes it).

#include "State.hpp"
#include "Metatable.hpp"
#include "Table.hpp"
#include "Basics.hpp"
#include "detail/ArgumentExtractor.hpp"
#include "detail/PushResult.hpp"

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

// Pops the value from the top of the stack and assigns it as a field on the
// global table @p tableName. If @p tableName does not exist or is not a table,
// the value is popped without effect.
void assignTopToTableField(lua_State* L,
                           const char* tableName,
                           const char* fieldName);

// Attaches an anonymous metatable carrying a single __call entry to the
// global table @p tableName. The metatable is never registered by name, so
// no entry is added to the Lua registry — keeping the registry clean of
// per-constructor scaffolding metatables.
void setCallMetatableOnGlobal(lua_State* L,
                              const char* tableName,
                              Basics::NativeFunction callFn);

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
		// Declare result with type R so reference-returning methods bind
		// to the original object; pushResult then uses std::forward<R> to
		// pick copy (for references) vs move (for owned locals).
		R result = (self->*Method)(
			ArgumentExtractor<std::tuple_element_t<I, ArgsTuple>>
				::extract(L, 2 + static_cast<int>(I))...);
		pushResult(L, std::forward<R>(result));
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

template <typename F> struct FreeFunctionTraits;

template <typename R, typename... Args>
struct FreeFunctionTraits<R (*)(Args...)> {
	using ReturnType = R;
	using ArgsTuple  = std::tuple<Args...>;
};

template <auto Fn, std::size_t... I>
int invokeFreeFunction(lua_State* lvm, std::index_sequence<I...>) {
	using Traits    = FreeFunctionTraits<decltype(Fn)>;
	using R         = typename Traits::ReturnType;
	using ArgsTuple = typename Traits::ArgsTuple;

	State L(lvm);

	if constexpr (std::is_void_v<R>) {
		Fn(ArgumentExtractor<std::tuple_element_t<I, ArgsTuple>>
			   ::extract(L, 1 + static_cast<int>(I))...);
		return 0;
	} else {
		R result = Fn(ArgumentExtractor<std::tuple_element_t<I, ArgsTuple>>
		                  ::extract(L, 1 + static_cast<int>(I))...);
		pushResult(L, std::forward<R>(result));
		return 1;
	}
}

template <auto Fn>
int freeFunctionWrapper(lua_State* lvm) {
	using Traits = FreeFunctionTraits<decltype(Fn)>;
	constexpr std::size_t N = std::tuple_size_v<typename Traits::ArgsTuple>;
	return invokeFreeFunction<Fn>(lvm, std::make_index_sequence<N>{});
}

} // namespace detail

template <typename T, typename... Args>
void Bind::constructor(State& state, const char* name) {
	state.createTable(name, [](Table&) { /* empty constructor table */ });
	int (*callFunc)(lua_State*) = [](lua_State* lvm) -> int {
		State L(lvm);
		detail::createInUserdata<T, Args...>(L, 2);
		L.assignMetaTable(Metatable<T>::metatableName());
		return 1;
	};
	detail::setCallMetatableOnGlobal(state.getState(), name, callFunc);
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

template <typename V>
void Bind::staticField(State& state, const char* tableName, const char* fieldName, V value) {
	detail::pushResult(state, std::forward<V>(value));
	detail::assignTopToTableField(state.getState(), tableName, fieldName);
}

template <auto Fn>
void Bind::staticFunction(State& state, const char* tableName, const char* funcName) {
	Basics::pushCFunction(state.getState(), &detail::freeFunctionWrapper<Fn>);
	detail::assignTopToTableField(state.getState(), tableName, funcName);
}

} // namespace Lua

#endif // LUACPP_BIND_HPP
