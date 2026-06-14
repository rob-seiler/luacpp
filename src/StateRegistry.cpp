#include <detail/StateRegistry.hpp>
#include <lua/lua.hpp>

#include <new>

namespace Lua {
namespace detail {

std::unordered_map<lua_State*, StateContext>           StateRegistry::s_contexts;
std::map<lua_State*, StateRegistry::DebugHook>         StateRegistry::s_debugHooks;

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
	lua_setwarnf(ctx->state, nullptr, nullptr);
	lua_State* state = ctx->state;
	s_debugHooks.erase(state);
	lua_close(state);          // may construct + destroy nested wrappers
	s_contexts.erase(state);   // invalidates `ctx`
}

void StateRegistry::setDebugHook(lua_State* state, DebugHook hook) {
	s_debugHooks[state] = std::move(hook);
}

StateRegistry::DebugHook StateRegistry::findDebugHook(lua_State* state) {
	auto it = s_debugHooks.find(state);
	if (it == s_debugHooks.end()) return {};
	return it->second;
}

} // namespace detail
} // namespace Lua
