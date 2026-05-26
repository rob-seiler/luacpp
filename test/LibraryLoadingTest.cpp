#include <gtest/gtest.h>

#include <luacpp/State.hpp>

#include <stdexcept>
#include <string>

namespace Lua {
namespace {

class LibraryLoadingTest : public ::testing::Test {};

template <typename T>
T readVar(State& s, const char* name) {
	auto v = s.readVariable<T>(name);
	if (!v) throw std::runtime_error(std::string("readVar: '") + name + "' missing or wrong type");
	return *v;
}

TEST_F(LibraryLoadingTest, openLibrary_singleLibIsAvailable) {
	State lua(State::LibMath);
	lua.loadAndExecuteScript("hasMath = (math ~= nil)");
	EXPECT_TRUE(readVar<bool>(lua, "hasMath"));
}

TEST_F(LibraryLoadingTest, openLibrary_unrequestedLibIsAbsent) {
	State lua(State::LibMath);
	lua.loadAndExecuteScript("hasIO = (io ~= nil)");
	EXPECT_FALSE(readVar<bool>(lua, "hasIO"));
}

TEST_F(LibraryLoadingTest, openLibrary_combinedMaskOpensExactlyThose) {
	State lua(State::LibMath | State::LibString | State::LibTable);
	lua.loadAndExecuteScript(R"(
		opened = (math ~= nil) and (string ~= nil) and (table ~= nil)
		closed = (io == nil) and (os == nil) and (coroutine == nil) and (debug == nil)
		result = opened and closed
	)");
	EXPECT_TRUE(readVar<bool>(lua, "result"));
}

TEST_F(LibraryLoadingTest, openLibrary_libAllOpensEverything) {
	State lua(State::LibAll);
	lua.loadAndExecuteScript(R"(
		result =
			math ~= nil and string ~= nil and table ~= nil and io ~= nil and
			os ~= nil and coroutine ~= nil and utf8 ~= nil and debug ~= nil and
			package ~= nil
	)");
	EXPECT_TRUE(readVar<bool>(lua, "result"));
}

TEST_F(LibraryLoadingTest, openLibrary_libDebugBitOpensDebugNotSomethingElse) {
	// Regression guard for the breaking bit reordering: pre-5.5 layout had
	// LibDebug at bit 9 and LibTable at bit 3. Now LibDebug is at bit 3.
	// This test pins that LibDebug really opens 'debug', not 'table' or 'utf8'.
	State lua(State::LibDebug);
	lua.loadAndExecuteScript(R"(
		isDebug = (debug ~= nil)
		isNotTable = (table == nil)
		isNotUTF8 = (utf8 == nil)
	)");
	EXPECT_TRUE(readVar<bool>(lua, "isDebug"));
	EXPECT_TRUE(readVar<bool>(lua, "isNotTable"));
	EXPECT_TRUE(readVar<bool>(lua, "isNotUTF8"));
}

TEST_F(LibraryLoadingTest, preloadLibrary_notAvailableUntilRequire) {
	// LibPackage is needed for require() to exist on the script side.
	State lua(State::LibPackage);
	lua.preloadLibrary(State::LibMath);

	lua.loadAndExecuteScript("beforeRequire = (math == nil)");
	EXPECT_TRUE(readVar<bool>(lua, "beforeRequire"))
	    << "preloaded library must not be a global until require() is called";

	lua.loadAndExecuteScript(
		"local m = require('math'); afterRequire = (m.sqrt ~= nil)");
	EXPECT_TRUE(readVar<bool>(lua, "afterRequire"));
}

TEST_F(LibraryLoadingTest, preloadLibrary_combinedMask) {
	State lua(State::LibPackage);
	lua.preloadLibrary(State::LibMath | State::LibString);

	lua.loadAndExecuteScript(R"(
		local m = require('math')
		local s = require('string')
		result = (m.sqrt ~= nil) and (s.upper ~= nil)
	)");
	EXPECT_TRUE(readVar<bool>(lua, "result"));
}

TEST_F(LibraryLoadingTest, openLibrary_andPreloadLibrary_coexist) {
	// Open math eagerly; preload string for lazy loading.
	State lua(State::LibPackage | State::LibMath);
	lua.preloadLibrary(State::LibString);

	lua.loadAndExecuteScript(R"(
		mathReady = (math ~= nil)
		stringInitiallyAbsent = (string == nil)
		local s = require('string')
		stringNowReady = (s.upper ~= nil)
	)");
	EXPECT_TRUE(readVar<bool>(lua, "mathReady"));
	EXPECT_TRUE(readVar<bool>(lua, "stringInitiallyAbsent"));
	EXPECT_TRUE(readVar<bool>(lua, "stringNowReady"));
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_prependsToPackagePath) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/myapp/scripts/?.lua");

	lua.loadAndExecuteScript("p = package.path");
	const auto path = readVar<std::string>(lua, "p");
	EXPECT_EQ(path.substr(0, 20), "/myapp/scripts/?.lua")
	    << "pattern must be prepended to package.path";
	EXPECT_EQ(path[20], ';') << "and separated from existing entries by ';'";
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_forNativeModuleAffectsCPath) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/myapp/native/?.so", /*forNativeModule=*/true);

	lua.loadAndExecuteScript("c = package.cpath");
	const auto cpath = readVar<std::string>(lua, "c");
	EXPECT_EQ(cpath.substr(0, 18), "/myapp/native/?.so")
	    << "pattern must be prepended to package.cpath when forNativeModule=true";
}

TEST_F(LibraryLoadingTest, addModuleSearchPath_pathAndCPathStayIndependent) {
	State lua(State::LibPackage);
	lua.addModuleSearchPath("/lua-only/?.lua");

	lua.loadAndExecuteScript(R"(
		p = package.path
		c = package.cpath
	)");
	const auto path = readVar<std::string>(lua, "p");
	const auto cpath = readVar<std::string>(lua, "c");

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

	lua.loadAndExecuteScript("p = package.path");
	const auto path = readVar<std::string>(lua, "p");
	const auto firstPos = path.find("/first/");
	const auto secondPos = path.find("/second/");

	ASSERT_NE(firstPos, std::string::npos);
	ASSERT_NE(secondPos, std::string::npos);
	EXPECT_LT(secondPos, firstPos)
	    << "most recently added pattern should be searched first";
}

} // namespace
} // namespace Lua
