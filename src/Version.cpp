#include <Version.hpp>
#include <lua/lua.hpp>
#include <cstdlib>

namespace Lua {

Version Version::getLuaVersion() {
	return Version(
		static_cast<uint8_t>(atoi(LUA_VERSION_MAJOR)),
		static_cast<uint8_t>(atoi(LUA_VERSION_MINOR)),
		static_cast<uint8_t>(atoi(LUA_VERSION_RELEASE))
	);
}

Version Version::readLuaVersion(lua_State* L) {
	const lua_Number version = lua_version(L);

	const uint8_t major = static_cast<uint8_t>(version / 100);
	const uint8_t minor = static_cast<uint8_t>(version - major * 100);
	const uint8_t patch = static_cast<uint8_t>(255); //lua_version doesn't return the patch version
	return Version(major, minor, patch);
}

} // namespace Lua