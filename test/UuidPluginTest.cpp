#include <gtest/gtest.h>

#include <luacpp/State.hpp>

#include <chrono>
#include <string>
#include <thread>

namespace Lua {
namespace {

#ifdef LUACPP_HAVE_UUID_MODULE

class UuidPluginTest : public ::testing::Test {};

// Helper: register the build's plugin dir on the state's cpath so that
// require("uuid") can locate the native module. Operates on a caller-owned
// State because State is non-movable (registerMethod captures `this` in Lua
// closures — see the State header for rationale).
//
// Lua's package.cpath patterns match the platform-specific shared-library
// extension: .dll on Windows, .so elsewhere (including macOS — Lua's default
// cpath uses .so even on macOS).
static void addUuidModuleSearchPath(State& lua) {
#ifdef _WIN32
	lua.addModuleSearchPath(LUACPP_UUID_MODULE_DIR "/?.dll", /*forNativeModule=*/true);
#else
	lua.addModuleSearchPath(LUACPP_UUID_MODULE_DIR "/?.so", /*forNativeModule=*/true);
#endif
}

TEST_F(UuidPluginTest, v4_returnsRfc4122FormattedString) {
	State lua(State::LibBase | State::LibPackage);
	addUuidModuleSearchPath(lua);

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
	State lua(State::LibBase | State::LibPackage);
	addUuidModuleSearchPath(lua);

	ASSERT_EQ(lua.loadAndExecuteScript(R"(
		uuid = require("uuid")
		a = uuid.v7()
	)"), 0);

	// RFC 9562 v7 is monotonic across millisecond boundaries; within a
	// single ms the trailing random bytes determine ordering, so two calls
	// close enough in time can lex-order either way. Sleep past one ms
	// boundary to make the comparison below deterministic.
	std::this_thread::sleep_for(std::chrono::milliseconds(2));

	ASSERT_EQ(lua.loadAndExecuteScript("b = uuid.v7()"), 0);

	const auto a = lua.readVariable<std::string>("a");
	const auto b = lua.readVariable<std::string>("b");

	ASSERT_EQ(a.size(), 36u);
	ASSERT_EQ(b.size(), 36u);
	EXPECT_EQ(a[14], '7');
	EXPECT_EQ(b[14], '7');
	EXPECT_LT(a, b) << "v7 IDs from different ms must be strictly lex-ordered";
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
