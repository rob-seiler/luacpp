#include <gtest/gtest.h>

#include <luacpp/State.hpp>

#include <string>

namespace Lua {
namespace {

#ifdef LUACPP_HAVE_UUID_MODULE

class UuidPluginTest : public ::testing::Test {};

// Helper: create a state ready to require("uuid") from the build's plugin dir.
// Defined as a function (not a fixture method) because State has no copy ctor;
// returning by move keeps each test's state independent.
static State makeStateWithUuidLoadable() {
	State lua(State::LibBase | State::LibPackage);
	lua.addModuleSearchPath(LUACPP_UUID_MODULE_DIR "/?.so", /*forNativeModule=*/true);
	return lua;
}

TEST_F(UuidPluginTest, v4_returnsRfc4122FormattedString) {
	State lua = makeStateWithUuidLoadable();

	ASSERT_EQ(lua.loadAndExecuteScript(R"(
		local uuid = require("uuid")
		id = uuid.v4()
	)"), 0);

	const auto id = lua.readVariable<std::string>("id");
	ASSERT_EQ(id.size(), 36u) << "got: " << id;
	EXPECT_EQ(id[8], '-');
	EXPECT_EQ(id[13], '-');
	EXPECT_EQ(id[18], '-');
	EXPECT_EQ(id[23], '-');
	EXPECT_EQ(id[14], '4') << "version nibble must be '4'";
	EXPECT_TRUE(id[19] == '8' || id[19] == '9' || id[19] == 'a' || id[19] == 'b')
	    << "variant nibble must be 8, 9, a or b; got '" << id[19] << "'";
}

TEST_F(UuidPluginTest, v7_hasVersion7AndTimeOrdering) {
	State lua = makeStateWithUuidLoadable();

	ASSERT_EQ(lua.loadAndExecuteScript(R"(
		local uuid = require("uuid")
		a = uuid.v7()
		b = uuid.v7()
	)"), 0);

	const auto a = lua.readVariable<std::string>("a");
	const auto b = lua.readVariable<std::string>("b");

	ASSERT_EQ(a.size(), 36u);
	ASSERT_EQ(b.size(), 36u);
	EXPECT_EQ(a[14], '7');
	EXPECT_EQ(b[14], '7');
	// RFC 9562 v7: 48-bit Unix-time-ms prefix makes lexicographic order
	// monotonic for IDs generated within the same ms or later.
	EXPECT_LE(a, b) << "v7 IDs must be lexicographically non-decreasing";
}

TEST_F(UuidPluginTest, requireFailsWithoutCustomCPath) {
	// Negative control: without addModuleSearchPath, Lua's default cpath
	// does not contain our build dir, so require("uuid") must fail. Confirms
	// that the positive tests above genuinely exercise the path we set.
	State lua(State::LibBase | State::LibPackage);
	const int rc = lua.loadAndExecuteScript("loaded = require('uuid')");
	EXPECT_NE(rc, 0) << "require should fail when the module path is not registered";
}

#endif // LUACPP_HAVE_UUID_MODULE

} // namespace
} // namespace Lua
