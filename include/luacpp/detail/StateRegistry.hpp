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
// Intrusive refcount drives lifecycle: the last release calls lua_close
// and erases the entry from the registry. The `closing` flag guards
// against re-entrant lua_close from __gc-driven wrappers that briefly
// acquire and release the context during finalization.
struct StateContext {
	lua_State* const state;
	ErrorPolicy      policy;
	DebugHook        debugHook;       // empty when none installed
	unsigned         refCount;
	bool             closing;

	// Warning subsystem state — lives with the VM (not the registering
	// State) because lua_setwarnf is VM-global and the trampoline's ud
	// points here. Any wrapper can configure the sink for all wrappers,
	// same as the error policy.
	std::unique_ptr<WarningLogger> warningLogger;
	std::string                    warningBuffer;
	bool                           warningsEnabled        = false;
	bool                           warningInProgress      = false;
	bool                           warningCurrentIsSingle = false;

	explicit StateContext(lua_State* s) noexcept
		: state(s), refCount(0), closing(false) {}

	StateContext(const StateContext&) = delete;
	StateContext& operator=(const StateContext&) = delete;

	// Multi-piece warn() assembly + @on/@off control. Called from the
	// lua_setwarnf trampoline with ud = this.
	void handleWarning(const char* msg, int tocont);
};

// lua_WarnFunction-shaped trampoline; forwards to ctx->handleWarning.
void warningTrampoline(void* ud, const char* msg, int tocont);

// Process-global, VM-keyed bookkeeping. All members static; no instance
// needed and none constructed. Internal to luacpp — not re-exported by
// the module surface.
class StateRegistry {
public:
	// Create a fresh lua_State via luaL_newstate. Throws std::bad_alloc
	// on failure. Exposed here so both State ctors can funnel through
	// the registry symmetrically (owning ctor passes this through to
	// acquire, borrowed ctor passes the user's lua_State).
	static lua_State* newVM();

	// Look up or create the context for `state`, incrementing refCount.
	// Sets `isMain` to true if this call inserted the entry — the caller
	// is then the wrapper that holds per-instance state (registerMethod's
	// `this` upvalue, the warning trampoline's ud). On allocation failure
	// during insert, closes `state` and rethrows: a failed wrapper
	// construction takes the lua_State down with it (transfer-of-ownership
	// semantics).
	static StateContext* acquire(lua_State* state, bool& isMain);

	// Decrement refCount; when it reaches zero, close the VM and erase
	// the map entry. luacpp always closes — both State ctors transfer
	// ownership of the lua_State into the context.
	static void release(StateContext* ctx) noexcept;

	// Non-owning lookup. Returns nullptr when no context exists for the
	// given lua_State. Used by the debug-hook trampoline to reach the
	// installed hook without taking a refcount.
	static StateContext* find(lua_State* state) noexcept;

	// Bump refCount on an already-resolved context. Used by the
	// State(StateContext*, lua_State*) private ctor to skip the second
	// mainThreadOf+lookup that a plain acquire(state) would do, when the
	// caller (typically a Lua trampoline) already has the ctx from find().
	static StateContext* retain(StateContext* ctx) noexcept;

private:
	static std::unordered_map<lua_State*, StateContext> s_contexts;
};

} // namespace detail
} // namespace Lua

#endif // LUACPP_DETAIL_STATEREGISTRY_HPP
