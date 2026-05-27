#ifndef LUACPP_STACKGUARD_HPP
#define LUACPP_STACKGUARD_HPP

#include "Basics.hpp"

struct lua_State;

namespace Lua {

/**
 * @brief RAII pop of N values from the Lua stack on scope exit.
 *
 * Replaces hand-written `lua_pop` / `popStack` cleanup so that intermediate
 * values are released on *every* exit path, including when a C++ exception
 * unwinds through the frame — e.g. a user-supplied callback throwing, or a
 * Lua error surfacing as a C++ exception (see the C++-compilation note below).
 *
 * Usage patterns:
 *  - "cleanup" — construct after pushing; the dtor pops at scope end:
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
 *      workOnTable(table);          // throws? guard pops the partial table
 *      lua_setglobal(m_state, name);
 *      guard.release();             // committed — nothing to pop
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
	    : m_state(state), m_count(popCount) {}

	~StackGuard() noexcept {
		if (m_count > 0) {
			Basics::popStack(m_state, m_count);
		}
	}

	StackGuard(const StackGuard&) = delete;
	StackGuard& operator=(const StackGuard&) = delete;
	StackGuard(StackGuard&&) = delete;
	StackGuard& operator=(StackGuard&&) = delete;

	/// The value(s) were consumed by Lua (setglobal/setfield/ref) — no pop.
	void release() noexcept { m_count = 0; }

	/// Account for additional pushed values the guard should also pop.
	void grow(int n = 1) noexcept { m_count += n; }

private:
	lua_State* m_state;
	int m_count;
};

} // namespace Lua

#endif // LUACPP_STACKGUARD_HPP
