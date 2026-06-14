#include <detail/StateRegistry.hpp>
#include <lua/lua.hpp>

#include <new>

namespace Lua {
namespace detail {

std::unordered_map<lua_State*, StateContext> StateRegistry::s_contexts;

lua_State* StateRegistry::newVM() {
	lua_State* state = luaL_newstate();
	if (!state) throw std::bad_alloc{};
	return state;
}

StateContext* StateRegistry::acquire(lua_State* state, bool& isMain) {
	auto it = s_contexts.find(state);
	if (it != s_contexts.end()) {
		isMain = false;
		++it->second.refCount;
		return &it->second;
	}
	// First wrapper for this state — create the entry and become the main.
	// If anything throws during the insert/setup, close `state` (the caller
	// has lost the chance to do so itself by the time we throw).
	try {
		auto [iter, inserted] = s_contexts.try_emplace(state, state);
		iter->second.policy.logger = std::make_unique<StreamLogger>();
		isMain = true;
		++iter->second.refCount;
		return &iter->second;
	} catch (...) {
		s_contexts.erase(state);
		lua_close(state);
		throw;
	}
}

void StateRegistry::release(StateContext* ctx) noexcept {
	if (--ctx->refCount != 0) return;
	// Re-entry guard: lua_close runs __gc finalizers, which may construct
	// State wrappers that acquire and release this same context. Without
	// the flag, those nested releases would re-enter lua_close.
	if (ctx->closing) return;

	ctx->closing = true;
	// Detach both Lua-side callbacks before close so finalizers can't trigger
	// our trampolines on a half-destroyed context.
	lua_setwarnf(ctx->state, nullptr, nullptr);
	lua_sethook (ctx->state, nullptr, 0, 0);

	lua_State* state = ctx->state;
	lua_close(state);         // may construct + destroy nested wrappers
	s_contexts.erase(state);  // invalidates `ctx`
}

StateContext* StateRegistry::find(lua_State* state) noexcept {
	auto it = s_contexts.find(state);
	return it != s_contexts.end() ? &it->second : nullptr;
}

} // namespace detail
} // namespace Lua
