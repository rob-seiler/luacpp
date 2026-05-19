#include <gtest/gtest.h>

#include <luacpp/Version.hpp>

#include <lua/lua.hpp> //needed to compare the version

namespace Lua {

TEST(VersionTest, EncodesComponents) {
	constexpr Version v{1, 2, 3};
	constexpr auto str = v.asString();

	EXPECT_EQ(v.getMajor(), 1);
	EXPECT_EQ(v.getMinor(), 2);
	EXPECT_EQ(v.getPatch(), 3);
	EXPECT_EQ(v.toNumber(), 0x010203u);
	EXPECT_EQ(v.toLuaNumber(), 10203u);
	EXPECT_STREQ(str.data(), "1.2.3");
}

TEST(VersionTest, FormatsTwoDigitComponents) {
	// asString() has a separate branch for components > 9 — cover it explicitly.
	constexpr Version v{5, 4, 10};
	EXPECT_STREQ(v.asString().data(), "5.4.10");
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
