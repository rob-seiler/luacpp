#include <detail/StateRegistry.hpp>
#include <lua/lua.hpp>

#include <new>

namespace Lua {
namespace detail {

std::unordered_map<lua_State*, StateContext> StateRegistry::s_contexts;

namespace {
// Canonical VM key: a coroutine sub-thread has its own lua_State*, but all
// threads of a VM share the registry. Keying by main thread lets sub-thread
// wrappers (e.g. inside Lua callbacks) join the existing context instead of
// registering a sibling. Falls back to `state` for malformed VMs.
lua_State* mainThreadOf(lua_State* state) {
	lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* main = lua_tothread(state, -1);
	lua_pop(state, 1);
	return main ? main : state;
}
} // namespace

lua_State* StateRegistry::newVM() {
	lua_State* state = luaL_newstate();
	if (!state) throw std::bad_alloc{};
	return state;
}

StateContext* StateRegistry::acquire(lua_State* state, bool& isMain) {
	lua_State* key = mainThreadOf(state);
	auto it = s_contexts.find(key);
	if (it != s_contexts.end()) {
		isMain = false;
		++it->second.refCount;
		return &it->second;
	}
	try {
		auto [iter, inserted] = s_contexts.try_emplace(key, key);
		iter->second.policy.logger = std::make_unique<StreamLogger>();
		isMain = true;
		++iter->second.refCount;
		return &iter->second;
	} catch (...) {
		s_contexts.erase(key);
		// Close-on-throw only for the ownership-transfer case (caller gave
		// us the main thread). Sub-thread callers don't own the VM, and
		// lua_close on a sub-thread is UB per Lua's docs anyway.
		if (state == key) {
			lua_close(state);
		}
		throw;
	}
}

void StateRegistry::release(StateContext* ctx) noexcept {
	if (--ctx->refCount != 0) return;
	// Guard against re-entry from __gc finalizers that construct + drop a
	// transient wrapper during lua_close.
	if (ctx->closing) return;

	ctx->closing = true;
	// Detach Lua-side callbacks before close so finalizers can't trigger
	// our trampolines on a half-destroyed context.
	lua_setwarnf(ctx->state, nullptr, nullptr);
	lua_sethook (ctx->state, nullptr, 0, 0);

	lua_State* state = ctx->state;
	lua_close(state);
	s_contexts.erase(state);   // invalidates `ctx`
}

StateContext* StateRegistry::find(lua_State* state) noexcept {
	auto it = s_contexts.find(mainThreadOf(state));
	return it != s_contexts.end() ? &it->second : nullptr;
}

StateContext* StateRegistry::retain(StateContext* ctx) noexcept {
	++ctx->refCount;
	return ctx;
}

void StateContext::handleWarning(const char* msg, int tocont) {
	// Mirror Lua's checkcontrol: a leading '@' is a control directive only
	// when the warning arrived in one piece. warningInProgress (not
	// warningBuffer.empty()) marks "first piece" so an empty first piece
	// can't be confused with a fresh start.
	if (!warningInProgress) {
		warningCurrentIsSingle = (tocont == 0);
		warningInProgress = true;
	}

	warningBuffer.append(msg);
	if (tocont) return;

	// Terminal piece: swap out the buffer so the next warning starts fresh
	// even if the logger throws.
	warningInProgress = false;
	std::string assembled;
	assembled.swap(warningBuffer);

	// @on / @off toggle reporting; other single-piece @-messages are
	// silently dropped (matches Lua's default warn function).
	if (warningCurrentIsSingle && !assembled.empty() && assembled.front() == '@') {
		if      (assembled == "@on")  warningsEnabled = true;
		else if (assembled == "@off") warningsEnabled = false;
		return;
	}

	if (warningsEnabled && warningLogger) {
		warningLogger->log(assembled);
	}
}

void warningTrampoline(void* ud, const char* msg, int tocont) {
	if (!ud || !msg) return;
	static_cast<StateContext*>(ud)->handleWarning(msg, tocont);
}

} // namespace detail
} // namespace Lua
