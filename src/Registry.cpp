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

Registry::ErrorCode Registry::loadString(lua_State* state, const char* src) {
	return static_cast<ErrorCode>(luaL_loadstring(state, src));
}

Type Registry::getRegistryTable(lua_State* state) {
	return static_cast<Type>(lua_gettable(state, LUA_REGISTRYINDEX));
}

void Registry::setRegistryTable(lua_State* state) {
	lua_settable(state, LUA_REGISTRYINDEX);
}

} // namespace Lua