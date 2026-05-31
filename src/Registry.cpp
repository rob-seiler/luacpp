#include <Registry.hpp>
#include <lua/lua.hpp>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>
#include <type_traits>

namespace Lua {

namespace {
// std::filesystem::path::u8string() returns std::string in C++17 but
// std::u8string in C++20+. char and char8_t are byte-compatible by design,
// so we reinterpret in the C++20 branch rather than maintain two impls.
// if constexpr keeps both branches in a single function across standards.
//
// This indirection becomes load-bearing once the library migrates its
// minimum standard to C++20 (planned alongside the module-wrapper work)
// — until then it is a forward-compatibility insurance policy and is
// equivalent to a plain copy on every C++17 toolchain we target.
std::string pathToUtf8(const std::filesystem::path& p) {
	auto u8 = p.u8string();
	if constexpr (std::is_same_v<decltype(u8), std::string>) {
		return u8;
	} else {
		// C++20+: std::u8string. Byte-equivalent to std::string under
		// the UTF-8 contract path::u8string() guarantees.
		return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
	}
}
} // namespace

// Status / LUA_* pinning lives in ErrorHandling.cpp now (the canonical home
// of LuaError::Status). No duplicate static_asserts here.

Registry::Registry(lua_State* L) : Table(L, LUA_REGISTRYINDEX, false) {}

LuaError::Status Registry::loadScript(Generic key, const char* src) {
	switch (key.getType()) {
		case Type::Boolean: return loadScript(key.get<bool>(), src);
		case Type::Number:
			if (key.isInteger()) {
				return loadScript(key.get<int64_t>(), src);
			}
			return loadScript(key.get<double>(), src);
		case Type::String: return loadScript(key.get<std::string>().c_str(), src);
		default: return LuaError::Status::InvalidKey;
	}
}

LuaError::Status Registry::loadScriptFromFile(Generic key, const std::filesystem::path& path) {
	switch (key.getType()) {
		case Type::Boolean: return loadScriptFromFile(key.get<bool>(), path);
		case Type::Number:
			if (key.isInteger()) {
				return loadScriptFromFile(key.get<int64_t>(), path);
			}
			return loadScriptFromFile(key.get<double>(), path);
		case Type::String: return loadScriptFromFile(key.get<std::string>().c_str(), path);
		default: return LuaError::Status::InvalidKey;
	}
}

LuaError::Status Registry::getScript(Generic key) {
	switch (key.getType()) {
		case Type::Boolean: return getScript(key.get<bool>());
		case Type::Number:
			if (key.isInteger()) {
				return getScript(key.get<int64_t>());
			}
			return getScript(key.get<double>());
		case Type::String: return getScript(key.get<std::string>().c_str());
		default: return LuaError::Status::InvalidKey;
	}
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

LuaError::Status Registry::loadString(lua_State* state, const char* src) {
	return static_cast<LuaError::Status>(luaL_loadstring(state, src));
}

LuaError::Status Registry::loadFile(lua_State* state, const std::filesystem::path& path) {
	// We bypass luaL_loadfile and read the file ourselves so non-ASCII paths
	// work on Windows:
	//   - luaL_loadfile uses fopen, which on Windows accepts only the active
	//     code page — std::filesystem::path::string() likewise narrows to the
	//     ACP, so any path containing characters outside the ACP fails to open
	//     even if the file exists.
	//   - std::ifstream taking a std::filesystem::path (C++17) is required to
	//     route through the platform's native API. The MSVC STL implements
	//     this via _wfopen, which accepts any UTF-16 path the OS can name.
	// We then hand the loaded bytes to luaL_loadbufferx with the path's UTF-8
	// form as the chunk name (prefixed with '@', the Lua convention that marks
	// the source as a file path), so tracebacks reference the path correctly
	// regardless of the system's narrow encoding.
	std::ifstream stream(path, std::ios::binary);
	const std::string u8path = pathToUtf8(path);
	const std::string chunkname = "@" + u8path;

	if (!stream) {
		// Match Lua's own ERRFILE message shape: "cannot open <path>: <reason>".
		// errno is set by the underlying fopen/_wfopen on every STL we target;
		// if it happens to be stale, the path piece — the part our tests pin —
		// is still correct. std::error_code sidesteps the strerror deprecation
		// warning MSVC emits at /W4.
		const std::error_code ec(errno, std::generic_category());
		const std::string reason = ec.message();
		lua_pushfstring(state, "cannot open %s: %s", u8path.c_str(), reason.c_str());
		return LuaError::Status::FileError;
	}

	std::ostringstream buf;
	buf << stream.rdbuf();
	std::string contents = std::move(buf).str();

	// Strip a leading UTF-8 BOM if present. luaL_loadfile does this implicitly;
	// without it, Lua's tokenizer would choke on the 0xEF byte. Notepad on
	// Windows writes the BOM by default for UTF-8 files.
	static constexpr char utf8Bom[3] = {'\xEF', '\xBB', '\xBF'};
	if (contents.size() >= 3 && std::memcmp(contents.data(), utf8Bom, 3) == 0) {
		contents.erase(0, 3);
	}

	return static_cast<LuaError::Status>(
		luaL_loadbufferx(state, contents.data(), contents.size(), chunkname.c_str(), nullptr));
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