#include <gtest/gtest.h>

#ifdef USE_CPP20_MODULES
#else
#include <luacpp/Version.hpp>
#endif

#include <lua/lua.hpp> //needed to compare the version

namespace Lua {

TEST(VersionTest, LuaCppVersion) {
	constexpr auto versionStr = LuaCppVersion.asString();

	EXPECT_EQ(LuaCppVersion.getMajor(), 0);
	EXPECT_EQ(LuaCppVersion.getMinor(), 1);
	EXPECT_EQ(LuaCppVersion.getPatch(), 0);
	EXPECT_EQ(LuaCppVersion.toNumber(), 0x000100);
	EXPECT_STREQ(versionStr.data(), "0.1.0");
}

TEST(VersionTest, getLuaVersion) {
	Version version = Version::getLuaVersion();

	EXPECT_EQ(version.getMajor(), atoi(LUA_VERSION_MAJOR));
	EXPECT_EQ(version.getMinor(), atoi(LUA_VERSION_MINOR));
	EXPECT_EQ(version.getPatch(), atoi(LUA_VERSION_RELEASE));
	EXPECT_EQ(version.toLuaNumber(), LUA_VERSION_RELEASE_NUM);
}

TEST(VersionTest, readLuaVersion) {
	Version version = Version::readLuaVersion();

	EXPECT_EQ(version.getMajor(), atoi(LUA_VERSION_MAJOR));
	EXPECT_EQ(version.getMinor(), atoi(LUA_VERSION_MINOR));
	EXPECT_EQ(version.getPatch(), 255);
}

} // namespace Lua<
