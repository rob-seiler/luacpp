#include <detail/StateRegistry.hpp>
#include <lua/lua.hpp>

#include <new>

namespace Lua {
namespace detail {

std::unordered_map<lua_State*, StateContext> StateRegistry::s_contexts;

namespace {
// Resolve `state` to the main thread of its VM. A coroutine sub-thread has
// its own lua_State*, but all threads of a VM share the registry; we key
// s_contexts by the main thread so wrappers built around a coroutine
// (typically inside a Lua C callback) find and join the existing context
// instead of registering the sub-thread as a sibling VM.
lua_State* mainThreadOf(lua_State* state) {
	lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* main = lua_tothread(state, -1);
	lua_pop(state, 1);
	// Fallback: a malformed state without LUA_RIDX_MAINTHREAD. Keep behaving
	// like the previous code path (treat the given state as canonical) so
	// we don't fail more loudly than the user's setup already does.
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
	// First wrapper for this VM — create the entry and become the main.
	// The canonical key is the main thread; sub-thread wrappers will find
	// the same entry via mainThreadOf().
	try {
		auto [iter, inserted] = s_contexts.try_emplace(key, key);
		iter->second.policy.logger = std::make_unique<StreamLogger>();
		isMain = true;
		++iter->second.refCount;
		return &iter->second;
	} catch (...) {
		s_contexts.erase(key);
		// Close-on-throw only when the caller passed the main thread itself —
		// that's the ownership-transfer case (State(Library) via newVM, or
		// State(L) wrapping a foreign main thread). When the caller passed
		// a coroutine sub-thread of a not-yet-known foreign VM, we don't
		// own the broader VM and must not close it; lua_close on a sub-
		// thread is UB per Lua's docs anyway.
		if (state == key) {
			lua_close(state);
		}
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
	auto it = s_contexts.find(mainThreadOf(state));
	return it != s_contexts.end() ? &it->second : nullptr;
}

StateContext* StateRegistry::retain(StateContext* ctx) noexcept {
	++ctx->refCount;
	return ctx;
}

void StateContext::handleWarning(const char* msg, int tocont) {
	// Lua's checkcontrol treats a leading '@' as a control directive only
	// when the warning arrived in one piece (first call has tocont == 0).
	// We mirror that: capture the first piece's tocont, consult it at the
	// terminal call. warningInProgress (not warningBuffer.empty()) decides
	// what counts as the first piece — an empty first piece would otherwise
	// leave the buffer empty and let the second piece masquerade as the
	// first.
	if (!warningInProgress) {
		warningCurrentIsSingle = (tocont == 0);
		warningInProgress = true;
	}

	warningBuffer.append(msg);
	if (tocont) return;

	// Terminal piece: reset the in-progress flag so the next warning starts
	// fresh, then take ownership of the assembled message in case the logger
	// throws.
	warningInProgress = false;
	std::string assembled;
	assembled.swap(warningBuffer);

	// @on / @off toggle reporting; any other single-piece @-message is
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
