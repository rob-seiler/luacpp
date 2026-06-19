#ifndef LUACPP_DETAIL_STATEREGISTRY_HPP
#define LUACPP_DETAIL_STATEREGISTRY_HPP

#include "../Debug.hpp"
#include "../ErrorHandling.hpp"
#include "../WarningHandling.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

struct lua_State;

namespace Lua {

class State;

namespace detail {

using DebugHook = std::function<void(State&, const DebugInfo&)>;

// Per-VM shared state for all State wrappers around the same lua_State.
// Lifetime is driven by an intrusive refCount; `closing` guards re-entry
// from __gc finalizers that briefly construct and drop transient wrappers
// inside lua_close.
struct StateContext {
	lua_State* const state;
	ErrorPolicy      policy;
	DebugHook        debugHook;        // empty when none installed
	unsigned         refCount;
	bool             closing;

	// Warning subsystem state lives here so any wrapper can configure the
	// sink (the trampoline's ud is StateContext*, not State*).
	std::unique_ptr<WarningLogger> warningLogger;
	std::string                    warningBuffer;
	bool                           warningsEnabled        = false;
	bool                           warningInProgress      = false;
	bool                           warningCurrentIsSingle = false;

	explicit StateContext(lua_State* s) noexcept
		: state(s), refCount(0), closing(false) {}

	StateContext(const StateContext&) = delete;
	StateContext& operator=(const StateContext&) = delete;

	// Multi-piece warn() assembly + @on/@off control.
	void handleWarning(const char* msg, int tocont);
};

// lua_WarnFunction-shaped trampoline; forwards to ctx->handleWarning.
void warningTrampoline(void* ud, const char* msg, int tocont);

// Process-global, VM-keyed bookkeeping. Internal to luacpp.
class StateRegistry {
public:
	// Fresh VM via luaL_newstate. Throws bad_alloc on failure.
	static lua_State* newVM();

	// Look up or create the context for `state`, ++refCount. Sets `isMain`
	// to true if this call inserted the entry. On insert failure, closes
	// `state` iff it was a main thread (sub-thread callers don't own the
	// broader VM); rethrows.
	static StateContext* acquire(lua_State* state, bool& isMain);

	// --refCount; on zero, close the VM and erase the entry.
	static void release(StateContext* ctx) noexcept;

	// Non-owning lookup. Returns nullptr when no context exists.
	static StateContext* find(lua_State* state) noexcept;

	// ++refCount on a ctx the caller already resolved (typically via
	// find() in a Lua trampoline). Lets the borrowed-view State ctor
	// skip a redundant mainThreadOf+map-lookup.
	static StateContext* retain(StateContext* ctx) noexcept;

private:
	static std::unordered_map<lua_State*, StateContext> s_contexts;
};

} // namespace detail
} // namespace Lua

#endif // LUACPP_DETAIL_STATEREGISTRY_HPP
