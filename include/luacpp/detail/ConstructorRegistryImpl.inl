#ifndef LUACPP_DETAIL_CONSTRUCTOR_REGISTRY_IMPL_INL
#define LUACPP_DETAIL_CONSTRUCTOR_REGISTRY_IMPL_INL

#include "../State.hpp"
#include "../Metatable.hpp"
#include "../Table.hpp"
#include "ArgumentExtractor.hpp"

#include <utility>
#include <string>

namespace Lua {
namespace detail {

template <typename T, typename... Args, size_t... I>
T* createInUserdataImpl(State& state, int startIdx, std::index_sequence<I...>) {
	return state.createUserData<T>(ArgumentExtractor<Args>::extract(state, startIdx + I)...);
}

template <typename T, typename... Args>
T* createInUserdata(State& state, int startIdx) {
	return createInUserdataImpl<T, Args...>(state, startIdx, std::index_sequence_for<Args...>{});
}

} // namespace detail

template <typename T, typename... Args>
void ConstructorRegistry::registerConstructor(State& state, const char* name) {
	state.createTable(name, [&state, name](Table& ctorTable) {
		std::string mtName = std::string(name) + "ConstructorMT";
		state.createMetaTable(mtName.c_str(), [](Table& mt) {
			int (*callFunc)(lua_State*) = [](lua_State* lvm) -> int {
				State L(lvm);
				// Construct directly in Lua userdata (no heap allocation + copy)
				T* obj = detail::createInUserdata<T, Args...>(L, 2);
				L.assignMetaTable(Metatable<T>::metatableName());
				return 1;
			};
			mt.setElement(State::MetaTable::Call, callFunc);
		});
		ctorTable.assignMetaTable(mtName.c_str());
	});
}

} // namespace Lua

#endif // LUACPP_DETAIL_CONSTRUCTOR_REGISTRY_IMPL_INL
