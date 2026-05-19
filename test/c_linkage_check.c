/*
 * Pure C translation unit: builds only if Lua's public API is exported
 * with C linkage. That is the contract LuaRocks-installed C modules rely
 * on — they include lua.h (with their own extern "C" wrap or via lua.hpp)
 * and link against unmangled lua_X and luaL_X symbols.
 *
 * If this TU fails to link, embedding any external C module via require()
 * would fail with the same kind of "unresolved external symbol" errors.
 * Compiling it as .c (not .cpp) guarantees the references emitted here
 * use C linkage; no extern "C" wrap is needed because everything is
 * already in C-land.
 */
#include <stdlib.h>
#include <string.h>

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

int main(void) {
	lua_State* L = luaL_newstate();
	if (!L) return 1;

	/* Exercise a handful of public API functions across lua_*, luaL_*, and
	 * a luaopen_*: enough to catch a linkage problem in any of the three
	 * exported namespaces. */
	luaL_openlibs(L);

	lua_pushinteger(L, 42);
	lua_pushstring(L, "hello");

	if (lua_tointeger(L, -2) != 42) { lua_close(L); return 2; }
	if (strcmp(lua_tostring(L, -1), "hello") != 0) { lua_close(L); return 3; }
	if (lua_gettop(L) != 2) { lua_close(L); return 4; }

	/* Roundtrip through a registered Lua function from a stdlib module
	 * (math.sqrt) to prove luaopen_math wiring works too. */
	lua_getglobal(L, "math");
	lua_getfield(L, -1, "sqrt");
	lua_pushinteger(L, 16);
	if (lua_pcall(L, 1, 1, 0) != LUA_OK) { lua_close(L); return 5; }
	if ((int)lua_tonumber(L, -1) != 4) { lua_close(L); return 6; }

	lua_close(L);
	return 0;
}
