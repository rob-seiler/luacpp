#ifndef LUACPP_DETAIL_CONSTRUCTOR_REGISTRY_IMPL_INL
#define LUACPP_DETAIL_CONSTRUCTOR_REGISTRY_IMPL_INL

#include "../State.hpp"
#include "../Metatable.hpp"
#include "../Table.hpp"
#include <utility>
#include <string>

namespace Lua {
namespace detail {

// Helper to check if a type is a class (userdata candidate)
template <typename T>
struct is_class_type {
	static constexpr bool value = std::is_class_v<std::remove_cv_t<std::remove_reference_t<T>>>;
};

// Helper to extract argument - handles userdata types
template <typename Arg, typename Enable = void>
struct ArgumentExtractor {
	static Arg extract(State& state, int index) {
		return state.getArgument<Arg>(index);
	}
};

// Specialization for class types (userdata)
template <typename Arg>
struct ArgumentExtractor<Arg, std::enable_if_t<is_class_type<Arg>::value>> {
	static Arg extract(State& state, int index) {
		using CleanArg = std::remove_cv_t<std::remove_reference_t<Arg>>;
		void* ud = Basics::checkUserData(state.getState(), index, Metatable<CleanArg>::metatableName());
		return *static_cast<CleanArg*>(ud);
	}
};

template <typename T, typename... Args, size_t... I>
T* createFromArgsImpl(State& state, int startIdx, std::index_sequence<I...>) {
	return new T(ArgumentExtractor<Args>::extract(state, startIdx + I)...);
}

template <typename T, typename... Args>
T* createFromArgs(State& state, int startIdx) {
	return createFromArgsImpl<T, Args...>(state, startIdx, std::index_sequence_for<Args...>{});
}

} // namespace detail

template <typename T, typename... Args>
void ConstructorRegistry::registerConstructor(State& state, const char* name) {
	state.createTable(name, [&state, name](Table& ctorTable) {
		std::string mtName = std::string(name) + "ConstructorMT";
		state.createMetaTable(mtName.c_str(), [](Table& mt) {
			int (*callFunc)(lua_State*) = [](lua_State* lvm) -> int {
				State L(lvm);
				T* obj = detail::createFromArgs<T, Args...>(L, 2);
				Metatable<T>::create(L, *obj);
				delete obj;
				return 1;
			};
			mt.setElement(State::MetaTable::Call, callFunc);
		});
		ctorTable.assignMetaTable(mtName.c_str());
	});
}

} // namespace Lua

#endif // LUACPP_DETAIL_CONSTRUCTOR_REGISTRY_IMPL_INL
