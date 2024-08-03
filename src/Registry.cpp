#include <Registry.hpp>
#include <lua/lua.hpp>

namespace Lua {

Registry::Registry(lua_State* L) : m_state(L) {}

Registry::ErrorCode Registry::loadScript(Generic key, const char* src) {
	switch (key.getType()) {
		case Type::Boolean: return loadScript(key.get<bool>(), src);
		case Type::Number: 
			if (key.isInteger()) {
				return loadScript(key.get<int64_t>(), src);
			}
			return loadScript(key.get<double>(), src);
		case Type::String: return loadScript(key.get<std::string>().c_str(), src);
		default: return ErrorCode::ErrorError;
	};
	return ErrorCode::RuntimeError;
}

Registry::ErrorCode Registry::getScript(Generic key) {
	switch (key.getType()) {
		case Type::Boolean: return getScript(key.get<bool>());
		case Type::Number: 
			if (key.isInteger()) {
				return getScript(key.get<int64_t>());
			}
			return getScript(key.get<double>());
		case Type::String: return getScript(key.get<std::string>().c_str());
		default: return ErrorCode::ErrorError;
	};
	return ErrorCode::RuntimeError;
}

bool Registry::copyContent(Registry& other) {
	lua_pushnil(other.m_state);
	while (lua_next(other.m_state, LUA_REGISTRYINDEX) != 0) {
		if (isUserDefinedEntry(other)) {
			copyEntry(other.m_state, m_state);
		}
		lua_pop(other.m_state, 1);
	}
	return true;
}

Registry::ErrorCode Registry::loadString(lua_State* state, const char* src) {
	return static_cast<ErrorCode>(luaL_loadstring(state, src));
}

Type Registry::getRegistryTable(lua_State* state) {	
	return static_cast<Type>(lua_rawget(state, LUA_REGISTRYINDEX));
}

void Registry::setRegistryTable(lua_State* state) {
	lua_rawset(state, LUA_REGISTRYINDEX);
}

bool Registry::isUserDefinedEntry(const Registry& registry) {
	int type = lua_type(registry.m_state, -2);
	switch (type) {
		case LUA_TSTRING:
			if (lua_tostring(registry.m_state, -2)[0] == '_') {
				return false;
			}
			break;
		case LUA_TNUMBER:
			break;
		default:
			return false;
	}

	type = lua_type(registry.m_state, -1);
	return type == LUA_TSTRING || type == LUA_TNUMBER || type == LUA_TBOOLEAN;
}

void Registry::copyEntry(lua_State* src, lua_State* dst) {
	//stack now contains: -1 => value; -2 => key

	if (lua_type(src, -2) == LUA_TSTRING) {
		lua_pushstring(dst, lua_tostring(src, -2));
	} else if (lua_type(src, -2) == LUA_TNUMBER) {
		lua_pushnumber(dst, lua_tonumber(src, -2));
	} else {
		return;
	}

	switch (lua_type(src, -1)) {
		case LUA_TSTRING:
			lua_pushstring(dst, lua_tostring(src, -1));
			break;
		case LUA_TNUMBER:
			lua_pushnumber(dst, lua_tonumber(src, -1));
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(dst, lua_toboolean(src, -1));
			break;
		default:
			lua_pop(dst, 1);
			return;
	}
	lua_rawset(dst, LUA_REGISTRYINDEX);
}

} // namespace Lua