/*
 * Mini UUID v4 / v7 plugin for luacpp. Bundled with the repository to act
 * as the require()-based smoke test for our LuaRocks-compatible C-linkage
 * exports — and to give downstream users a small, maintained UUID source
 * that ships with the library, independent of the LuaRocks ecosystem.
 *
 *   uuid.v4()  -> RFC 4122 random UUID, formatted as
 *                 "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
 *   uuid.v7()  -> RFC 9562 time-ordered UUID with a 48-bit Unix-time-ms
 *                 prefix; two close-in-time calls produce lexicographically
 *                 ordered results, which makes them attractive as database
 *                 keys or correlation IDs.
 *
 * Random source:
 *   POSIX   -> /dev/urandom (universal, no glibc-version dependency)
 *   Windows -> BCryptGenRandom (system CSPRNG)
 *
 * The current build only wires this up on POSIX because dlopen'd modules
 * on Windows cannot resolve lua_* references against the host executable
 * without a shared Lua DLL. The cross-platform code paths are kept so the
 * source stays portable once that build mode is added.
 */

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#  include <bcrypt.h>
#  pragma comment(lib, "bcrypt.lib")
#  define LUACPP_PLUGIN_EXPORT __declspec(dllexport)
#else
#  include <sys/time.h>
#  include <unistd.h>
#  include <fcntl.h>
#  define LUACPP_PLUGIN_EXPORT
#endif

#include <lua/lua.h>
#include <lua/lauxlib.h>

static int fill_random(unsigned char* buf, size_t n) {
#ifdef _WIN32
	return BCryptGenRandom(NULL, buf, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? 1 : 0;
#else
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) return 0;
	size_t got = 0;
	while (got < n) {
		ssize_t r = read(fd, buf + got, n - got);
		if (r < 0) { close(fd); return 0; }
		got += (size_t)r;
	}
	close(fd);
	return 1;
#endif
}

static uint64_t unix_time_ms(void) {
#ifdef _WIN32
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	/* FILETIME is in 100-ns ticks since 1601-01-01; offset to Unix epoch. */
	uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	return (t - 116444736000000000ULL) / 10000ULL;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
#endif
}

static void format_uuid(const unsigned char* bytes, char* out) {
	static const char hex[] = "0123456789abcdef";
	int o = 0;
	for (int i = 0; i < 16; ++i) {
		if (i == 4 || i == 6 || i == 8 || i == 10) out[o++] = '-';
		out[o++] = hex[bytes[i] >> 4];
		out[o++] = hex[bytes[i] & 0xF];
	}
	out[o] = '\0';
}

static int l_v4(lua_State* L) {
	unsigned char b[16];
	if (!fill_random(b, 16)) {
		return luaL_error(L, "uuid.v4: failed to obtain random bytes");
	}
	b[6] = (unsigned char)((b[6] & 0x0F) | 0x40); /* version 4 */
	b[8] = (unsigned char)((b[8] & 0x3F) | 0x80); /* variant 10 */
	char out[37];
	format_uuid(b, out);
	lua_pushlstring(L, out, 36);
	return 1;
}

static int l_v7(lua_State* L) {
	unsigned char b[16];
	if (!fill_random(b, 16)) {
		return luaL_error(L, "uuid.v7: failed to obtain random bytes");
	}
	uint64_t t = unix_time_ms();
	b[0] = (unsigned char)((t >> 40) & 0xFF);
	b[1] = (unsigned char)((t >> 32) & 0xFF);
	b[2] = (unsigned char)((t >> 24) & 0xFF);
	b[3] = (unsigned char)((t >> 16) & 0xFF);
	b[4] = (unsigned char)((t >> 8) & 0xFF);
	b[5] = (unsigned char)(t & 0xFF);
	b[6] = (unsigned char)((b[6] & 0x0F) | 0x70); /* version 7 */
	b[8] = (unsigned char)((b[8] & 0x3F) | 0x80); /* variant 10 */
	char out[37];
	format_uuid(b, out);
	lua_pushlstring(L, out, 36);
	return 1;
}

static const luaL_Reg uuid_fns[] = {
	{"v4", l_v4},
	{"v7", l_v7},
	{NULL, NULL}
};

LUACPP_PLUGIN_EXPORT int luaopen_uuid(lua_State* L) {
	luaL_newlib(L, uuid_fns);
	return 1;
}
