#ifndef LUACPP_DETAIL_STATEREGISTRY_HPP
#define LUACPP_DETAIL_STATEREGISTRY_HPP

#include "../Debug.hpp"
#include "../ErrorHandling.hpp"

#include <functional>
#include <map>
#include <unordered_map>

struct lua_State;

namespace Lua {

class State;

namespace detail {

// Per-VM shared state for all State wrappers around the same lua_State.
// Intrusive refcount drives lifecycle: the last release calls lua_close
// and erases the entry from the registry. The `closing` flag guards
// against re-entrant lua_close from __gc-driven wrappers that briefly
// acquire and release the context during finalization.
struct StateContext {
	lua_State* const state;
	ErrorPolicy      policy;
	unsigned         refCount;
	bool             closing;

	explicit StateContext(lua_State* s) noexcept
		: state(s), refCount(0), closing(false) {}

	StateContext(const StateContext&) = delete;
	StateContext& operator=(const StateContext&) = delete;
};

// Process-global, VM-keyed bookkeeping. All members static; no instance
// needed and none constructed. Internal to luacpp — not re-exported by
// the module surface.
class StateRegistry {
public:
	using DebugHook = std::function<void(State&, const DebugInfo&)>;

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

	// Debug-hook bookkeeping — keyed by lua_State*, lives next to the
	// context map because both are VM-keyed process-global state.
	static void      setDebugHook(lua_State* state, DebugHook hook);
	static DebugHook findDebugHook(lua_State* state);

private:
	static std::unordered_map<lua_State*, StateContext> s_contexts;
	static std::map<lua_State*, DebugHook>              s_debugHooks;
};

} // namespace detail
} // namespace Lua

#endif // LUACPP_DETAIL_STATEREGISTRY_HPP
