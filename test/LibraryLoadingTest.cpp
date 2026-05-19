#include <gtest/gtest.h>

#include <luacpp/State.hpp>

#include <string>

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

TEST_F(LibraryLoadingTest, addModuleSearchPath_prependsToPackagePath) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/myapp/scripts/?.lua");

	EXPECT_EQ(lua.loadAndExecuteScript("p = package.path"), 0);
	const auto path = lua.readVariable<std::string>("p");
	EXPECT_EQ(path.substr(0, 20), "/myapp/scripts/?.lua")
	    << "pattern must be prepended to package.path";
	EXPECT_EQ(path[20], ';') << "and separated from existing entries by ';'";
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_forNativeModuleAffectsCPath) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/myapp/native/?.so", /*forNativeModule=*/true);

	EXPECT_EQ(lua.loadAndExecuteScript("c = package.cpath"), 0);
	const auto cpath = lua.readVariable<std::string>("c");
	EXPECT_EQ(cpath.substr(0, 18), "/myapp/native/?.so")
	    << "pattern must be prepended to package.cpath when forNativeModule=true";
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_pathAndCPathStayIndependent) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/lua-only/?.lua");

	EXPECT_EQ(lua.loadAndExecuteScript(R"(
		p = package.path
		c = package.cpath
	)"), 0);
	const auto path = lua.readVariable<std::string>("p");
	const auto cpath = lua.readVariable<std::string>("c");

	EXPECT_NE(path.find("/lua-only/"), std::string::npos);
	EXPECT_EQ(cpath.find("/lua-only/"), std::string::npos)
	    << "modifying package.path must not bleed into package.cpath";
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_silentNoOpWithoutLibPackage) {
	State lua(State::LibNone);
	// Without LibPackage there is no package table; the call must neither
	// throw nor leave any value behind on the Lua stack.
	EXPECT_NO_THROW(lua.addModuleSearchPath("/somewhere/?.lua"));
	EXPECT_EQ(lua.getStackSize(), 0);
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_multipleCallsAccumulateLatestFirst) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/first/?.lua");
	lua.addModuleSearchPath("/second/?.lua");

	EXPECT_EQ(lua.loadAndExecuteScript("p = package.path"), 0);
	const auto path = lua.readVariable<std::string>("p");
	const auto firstPos = path.find("/first/");
	const auto secondPos = path.find("/second/");

	ASSERT_NE(firstPos, std::string::npos);
	ASSERT_NE(secondPos, std::string::npos);
	EXPECT_LT(secondPos, firstPos)
	    << "most recently added pattern should be searched first";
}

} // namespace
} // namespace Lua
