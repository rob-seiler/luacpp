#include <detail/Bind.hpp>
#include <Basics.hpp>

#include <lua/lua.hpp>

namespace Lua {
namespace detail {

namespace {

int propertyIndexDispatcher(lua_State* L) {
	// Stack: [self, key]
	if (!lua_getmetatable(L, 1)) {
		lua_pushnil(L);
		return 1;
	}
	// Stack: [self, key, metatable]

	lua_getfield(L, -1, "__methods");
	if (lua_istable(L, -1)) {
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
		if (!lua_isnil(L, -1)) {
			return 1;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "__getters");
	if (lua_istable(L, -1)) {
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
		if (lua_isfunction(L, -1)) {
			lua_pushvalue(L, 1);
			lua_call(L, 1, 1);
			return 1;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	lua_pushnil(L);
	return 1;
}

int propertyNewindexDispatcher(lua_State* L) {
	// Stack: [self, key, value]
	if (!lua_getmetatable(L, 1)) {
		return luaL_error(L, "userdata has no metatable");
	}

	lua_getfield(L, -1, "__setters");
	if (lua_istable(L, -1)) {
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
		if (lua_isfunction(L, -1)) {
			lua_pushvalue(L, 1);
			lua_pushvalue(L, 3);
			lua_call(L, 2, 0);
			return 0;
		}
		lua_pop(L, 1);
	}

	return luaL_error(L, "attempt to set unknown property '%s'",
	                  luaL_optstring(L, 2, "?"));
}

// Installs the __index/__newindex dispatchers and the three sub-tables
// (__methods, __getters, __setters) on the metatable currently at top of stack.
// Idempotent: only installs once per metatable. Leaves the metatable on the stack.
void ensureDispatchersInstalled(lua_State* L) {
	lua_getfield(L, -1, "__methods");
	bool alreadyInstalled = lua_istable(L, -1);
	lua_pop(L, 1);
	if (alreadyInstalled) {
		return;
	}

	lua_newtable(L);
	lua_setfield(L, -2, "__methods");
	lua_newtable(L);
	lua_setfield(L, -2, "__getters");
	lua_newtable(L);
	lua_setfield(L, -2, "__setters");

	lua_pushcfunction(L, propertyIndexDispatcher);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, propertyNewindexDispatcher);
	lua_setfield(L, -2, "__newindex");
}

} // anonymous namespace

void addMethodToMetatable(lua_State* L,
                          const char* metatableName,
                          const char* methodName,
                          Basics::NativeFunction func) {
	if (luaL_getmetatable(L, metatableName) != LUA_TTABLE) {
		lua_pop(L, 1);
		return;
	}
	ensureDispatchersInstalled(L);

	lua_getfield(L, -1, "__methods");
	lua_pushcfunction(L, func);
	lua_setfield(L, -2, methodName);
	lua_pop(L, 2); // __methods + metatable
}

void addPropertyToMetatable(lua_State* L,
                            const char* metatableName,
                            const char* propertyName,
                            Basics::NativeFunction getter,
                            Basics::NativeFunction setter) {
	if (luaL_getmetatable(L, metatableName) != LUA_TTABLE) {
		lua_pop(L, 1);
		return;
	}
	ensureDispatchersInstalled(L);

	lua_getfield(L, -1, "__getters");
	lua_pushcfunction(L, getter);
	lua_setfield(L, -2, propertyName);
	lua_pop(L, 1);

	lua_getfield(L, -1, "__setters");
	lua_pushcfunction(L, setter);
	lua_setfield(L, -2, propertyName);
	lua_pop(L, 1);

	lua_pop(L, 1); // metatable
}

} // namespace detail
} // namespace Lua
