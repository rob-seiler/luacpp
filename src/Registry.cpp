#include <Registry.hpp>
#include <lua/lua.hpp>

namespace Lua {

Registry::Registry(lua_State* L) : m_state(L) {}

Registry::ErrorCode Registry::loadScript(const char* name, const char* src) {
	ErrorCode res = static_cast<ErrorCode>(luaL_loadstring(m_state, src));
	if (res == ErrorCode::Ok) {
		lua_pushstring(m_state, name);
		lua_pushvalue(m_state, -2);
		lua_settable(m_state, LUA_REGISTRYINDEX);

		lua_pop(m_state, 1);
	}
	return res;
}

} // namespace Lua