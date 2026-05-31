#ifndef LUACPP_STACKGUARD_HPP
#define LUACPP_STACKGUARD_HPP

#include "Basics.hpp"

#include <cassert>

struct lua_State;

namespace Lua {

/**
 * @brief RAII restore of the Lua stack top.
 *
 * Records a target top at construction (current_top minus `popCount`) and
 * `lua_settop`s back to it on scope exit. The top-based design means the
 * guard restores to the *same* level regardless of intermediate pushes —
 * forgotten pops become non-fatal stack imbalances rather than accumulating
 * leaks, and corner cases like a Lua-API call that pushes partially before
 * erroring still leave a balanced stack.
 *
 * Usage patterns:
 *  - "cleanup" — construct after pushing; the dtor restores at scope end:
 *      @code
 *      pushGlobalToStack(name);
 *      StackGuard guard(m_state);   // pops the value on any exit
 *      // ... work that may throw ...
 *      @endcode
 *  - "commit" — call release() once the value has been handed to Lua (e.g.
 *    lua_setglobal/lua_setfield/luaL_ref consumed it), so the dtor is a no-op:
 *      @code
 *      lua_newtable(m_state);
 *      StackGuard guard(m_state);
 *      workOnTable(table);          // throws? guard restores the stack
 *      lua_setglobal(m_state, name);
 *      guard.release();             // committed — nothing to restore
 *      @endcode
 *
 * @note Exception-safety against *Lua* errors depends on Lua being compiled
 *       as C++ (lua_error -> C++ throw, not setjmp/longjmp). This project
 *       enforces that in extern/lua/CMakeLists.txt and already relies on it
 *       for the Guard in State::anchorOwned. Against plain C++ exceptions
 *       (user callbacks throwing) StackGuard is correct regardless.
 */
class StackGuard {
public:
	explicit StackGuard(lua_State* state, int popCount = 1) noexcept
	    : m_state(state),
	      m_target(Basics::getStackTop(state) - popCount),
	      m_active(true) {}

	~StackGuard() noexcept {
		if (m_active) {
			// Clamp at 0 — a target below the stack bottom is a request to
			// clear everything (settop(0)). lua_settop interprets negative
			// indices as relative-to-top and would no-op here otherwise.
			Basics::setStackTop(m_state, m_target < 0 ? 0 : m_target);
		}
	}

	StackGuard(const StackGuard&) = delete;
	StackGuard& operator=(const StackGuard&) = delete;
	StackGuard(StackGuard&&) = delete;
	StackGuard& operator=(StackGuard&&) = delete;

	/// The value(s) were consumed by Lua (setglobal/setfield/ref) — no restore.
	void release() noexcept { m_active = false; }

protected:
	lua_State* m_state;
	int        m_target;
	bool       m_active;
};

/**
 * @brief Extension of StackGuard that asserts (debug only) on intermediate
 *        stack imbalance — i.e. when code in the guard's scope pushed values
 *        without popping them.
 *
 * Catches the downside of plain top-restore: forgotten pops would otherwise
 * be silently cleaned up rather than surfacing as a bug. In release builds
 * the assertion is compiled out, behavior is identical to the base.
 *
 * Use this in code paths where push counts are predictable. For paths whose
 * intermediate state is intentionally unpredictable (e.g. wrapping
 * luaL_tolstring), either stick with plain StackGuard or call
 * tolerateImbalance() to suppress the check at this call site.
 *
 * Destructor order is what makes this work without virtual: ~Assertion runs
 * first (checks the stack), then ~StackGuard runs (restores it). No vtable.
 */
class AssertionStackGuard : public StackGuard {
public:
	explicit AssertionStackGuard(lua_State* state, int popCount = 1) noexcept
	    : StackGuard(state, popCount),
	      m_savedTop(Basics::getStackTop(state)) {}

	~AssertionStackGuard() noexcept {
#ifndef NDEBUG
		if (m_active && !m_imbalanceTolerated) {
			assert(Basics::getStackTop(m_state) == m_savedTop &&
			       "AssertionStackGuard: intermediate code left the Lua stack "
			       "unbalanced. Either fix the missing pop, or call "
			       "tolerateImbalance() if the imbalance is intentional.");
		}
#endif
		// ~StackGuard runs after this body, performing the actual settop.
	}

	/// Silences the debug balance check at this call site.
	void tolerateImbalance() noexcept { m_imbalanceTolerated = true; }

private:
	int  m_savedTop;
	bool m_imbalanceTolerated = false;
};

/**
 * @brief Build-flavor-selected default guard: AssertionStackGuard in debug
 *        builds (NDEBUG not defined), plain StackGuard in release. Lets
 *        internal code get debug bug-detection automatically without paying
 *        the 8-byte member overhead in release.
 *
 * Sites that need the lean variant unconditionally — e.g. popErrorFromStack
 * where partial intermediate state is intentional — should reference
 * StackGuard directly instead of going through this alias.
 */
#ifndef NDEBUG
using DefaultStackGuard = AssertionStackGuard;
#else
using DefaultStackGuard = StackGuard;
#endif

} // namespace Lua

#endif // LUACPP_STACKGUARD_HPP
