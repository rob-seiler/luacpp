#include <gtest/gtest.h>

#include <luacpp/State.hpp>

namespace Lua {
namespace {

class LibraryLoadingTest : public ::testing::Test {};

TEST_F(LibraryLoadingTest, openLibrary_singleLibIsAvailable) {
	State lua(State::LibMath);
	EXPECT_EQ(lua.loadAndExecuteScript("hasMath = (math ~= nil)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("hasMath"));
}

TEST_F(LibraryLoadingTest, openLibrary_unrequestedLibIsAbsent) {
	State lua(State::LibMath);
	EXPECT_EQ(lua.loadAndExecuteScript("hasIO = (io ~= nil)"), 0);
	EXPECT_FALSE(lua.readVariable<bool>("hasIO"));
}

TEST_F(LibraryLoadingTest, openLibrary_combinedMaskOpensExactlyThose) {
	State lua(State::LibMath | State::LibString | State::LibTable);
	EXPECT_EQ(lua.loadAndExecuteScript(R"(
		opened = (math ~= nil) and (string ~= nil) and (table ~= nil)
		closed = (io == nil) and (os == nil) and (coroutine == nil) and (debug == nil)
		result = opened and closed
	)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("result"));
}

TEST_F(LibraryLoadingTest, openLibrary_libAllOpensEverything) {
	State lua(State::LibAll);
	EXPECT_EQ(lua.loadAndExecuteScript(R"(
		result =
			math ~= nil and string ~= nil and table ~= nil and io ~= nil and
			os ~= nil and coroutine ~= nil and utf8 ~= nil and debug ~= nil and
			package ~= nil
	)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("result"));
}

TEST_F(LibraryLoadingTest, openLibrary_libDebugBitOpensDebugNotSomethingElse) {
	// Regression guard for the breaking bit reordering: pre-5.5 layout had
	// LibDebug at bit 9 and LibTable at bit 3. Now LibDebug is at bit 3.
	// This test pins that LibDebug really opens 'debug', not 'table' or 'utf8'.
	State lua(State::LibDebug);
	EXPECT_EQ(lua.loadAndExecuteScript(R"(
		isDebug = (debug ~= nil)
		isNotTable = (table == nil)
		isNotUTF8 = (utf8 == nil)
	)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("isDebug"));
	EXPECT_TRUE(lua.readVariable<bool>("isNotTable"));
	EXPECT_TRUE(lua.readVariable<bool>("isNotUTF8"));
}

TEST_F(LibraryLoadingTest, preloadLibrary_notAvailableUntilRequire) {
	// LibPackage is needed for require() to exist on the script side.
	State lua(State::LibPackage);
	lua.preloadLibrary(State::LibMath);

	EXPECT_EQ(lua.loadAndExecuteScript("beforeRequire = (math == nil)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("beforeRequire"))
	    << "preloaded library must not be a global until require() is called";

	EXPECT_EQ(lua.loadAndExecuteScript(
		"local m = require('math'); afterRequire = (m.sqrt ~= nil)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("afterRequire"));
}

TEST_F(LibraryLoadingTest, preloadLibrary_combinedMask) {
	State lua(State::LibPackage);
	lua.preloadLibrary(State::LibMath | State::LibString);

	EXPECT_EQ(lua.loadAndExecuteScript(R"(
		local m = require('math')
		local s = require('string')
		result = (m.sqrt ~= nil) and (s.upper ~= nil)
	)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("result"));
}

TEST_F(LibraryLoadingTest, openLibrary_andPreloadLibrary_coexist) {
	// Open math eagerly; preload string for lazy loading.
	State lua(State::LibPackage | State::LibMath);
	lua.preloadLibrary(State::LibString);

	EXPECT_EQ(lua.loadAndExecuteScript(R"(
		mathReady = (math ~= nil)
		stringInitiallyAbsent = (string == nil)
		local s = require('string')
		stringNowReady = (s.upper ~= nil)
	)"), 0);
	EXPECT_TRUE(lua.readVariable<bool>("mathReady"));
	EXPECT_TRUE(lua.readVariable<bool>("stringInitiallyAbsent"));
	EXPECT_TRUE(lua.readVariable<bool>("stringNowReady"));
}

} // namespace
} // namespace Lua
