#include <gtest/gtest.h>

#include <luacpp/State.hpp>

#include <lua/lua.hpp>  // for LUA_OK, LUA_ERRFILE, LUA_ERRSYNTAX, LUA_ERRRUN

#include <string>

#ifndef LUACPP_TEST_DATA_DIR
#error "LUACPP_TEST_DATA_DIR not defined; CMake target_compile_definitions missing"
#endif

namespace Lua {

namespace {
Lua::File dataFile(const char* name) {
	return Lua::File(LUACPP_TEST_DATA_DIR) / name;
}
}

TEST(FileLoadingTest, ValidFile_LoadsAndExecutes) {
	State lua;
	int status = lua.loadAndExecuteScript(dataFile("valid.lua"));

	ASSERT_EQ(status, LUA_OK);
	EXPECT_TRUE(lua.getErrorList().empty());
	EXPECT_EQ(lua.readVariable<int>("x"), 42);
	EXPECT_EQ(lua.readVariable<std::string>("greeting"), "hello from file");
}

TEST(FileLoadingTest, MissingFile_ReturnsErrFileAndCapturesPath) {
	State lua;
	int status = lua.loadAndExecuteScript(Lua::File("does_not_exist_xyz.lua"));

	ASSERT_EQ(status, LUA_ERRFILE);
	const auto& errors = lua.getErrorList();
	ASSERT_FALSE(errors.empty());
	// Lua's standard error format for ERRFILE is
	//   "cannot open <path>: <reason>"
	// We don't pin the entire message (varies by libc) but the path must be in it.
	EXPECT_NE(errors.front().find("does_not_exist_xyz.lua"), std::string::npos)
		<< "error message did not reference the failing path: " << errors.front();
}

TEST(FileLoadingTest, SyntaxError_ReturnsErrSyntaxAndReferencesFile) {
	State lua;
	int status = lua.loadAndExecuteScript(dataFile("syntax_error.lua"));

	ASSERT_EQ(status, LUA_ERRSYNTAX);
	const auto& errors = lua.getErrorList();
	ASSERT_FALSE(errors.empty());
	// chunkname (= '@path') means the file name appears in the error message
	// — that is the whole point of using luaL_dofile over reading the file
	// ourselves and calling luaL_dostring.
	EXPECT_NE(errors.front().find("syntax_error.lua"), std::string::npos)
		<< "syntax error did not reference the file: " << errors.front();
}

TEST(FileLoadingTest, RuntimeError_ReturnsErrRunAndPinpointsLine) {
	State lua;
	int status = lua.loadAndExecuteScript(dataFile("runtime_error.lua"));

	ASSERT_EQ(status, LUA_ERRRUN);
	const auto& errors = lua.getErrorList();
	ASSERT_FALSE(errors.empty());
	// runtime_error.lua calls error("boom...") on line 4. The traceback must
	// reference both the file and the line.
	EXPECT_NE(errors.front().find("runtime_error.lua"), std::string::npos)
		<< "runtime error did not reference the file: " << errors.front();
	EXPECT_NE(errors.front().find(":4"), std::string::npos)
		<< "runtime error did not reference line 4: " << errors.front();
}

TEST(FileLoadingTest, RegistryRoundtrip_LoadFromFileThenExecute) {
	State lua;
	const char* key = "stored_script";

	int loadStatus = lua.loadScript(key, dataFile("valid.lua"));
	ASSERT_EQ(loadStatus, static_cast<int>(Registry::ErrorCode::Ok));

	int execStatus = lua.executeScript(key);
	ASSERT_EQ(execStatus, LUA_OK);
	EXPECT_EQ(lua.readVariable<int>("x"), 42);
}

} // namespace Lua
