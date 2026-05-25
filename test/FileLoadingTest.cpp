#include <gtest/gtest.h>

#include <luacpp/State.hpp>

#include <lua/lua.hpp>  // for LUA_OK, LUA_ERRFILE, LUA_ERRSYNTAX, LUA_ERRRUN

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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
	auto& errors = lua.installErrorHandler<LogDecorator>();
	int status = lua.loadAndExecuteScript(dataFile("valid.lua"));

	ASSERT_EQ(status, LUA_OK);
	EXPECT_TRUE(errors.log().empty());
	EXPECT_EQ(lua.readVariable<int>("x"), 42);
	EXPECT_EQ(lua.readVariable<std::string>("greeting"), "hello from file");
}

TEST(FileLoadingTest, MissingFile_ReturnsErrFileAndCapturesPath) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	int status = lua.loadAndExecuteScript(Lua::File("does_not_exist_xyz.lua"));

	ASSERT_EQ(status, LUA_ERRFILE);
	ASSERT_FALSE(errors.log().empty());
	EXPECT_EQ(errors.log().front().category, LuaError::Category::Load);
	// Lua's standard error format for ERRFILE is
	//   "cannot open <path>: <reason>"
	// We don't pin the entire message (varies by libc) but the path must be in it.
	const auto& msg = errors.log().front().message;
	EXPECT_NE(msg.find("does_not_exist_xyz.lua"), std::string::npos)
		<< "error message did not reference the failing path: " << msg;
}

TEST(FileLoadingTest, SyntaxError_ReturnsErrSyntaxAndReferencesFile) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	int status = lua.loadAndExecuteScript(dataFile("syntax_error.lua"));

	ASSERT_EQ(status, LUA_ERRSYNTAX);
	ASSERT_FALSE(errors.log().empty());
	EXPECT_EQ(errors.log().front().category, LuaError::Category::Load);
	// chunkname (= '@path') means the file name appears in the error message
	// — that is the whole point of the file-source overload vs. piping the
	// file's bytes through loadAndExecuteScript(const char*).
	const auto& msg = errors.log().front().message;
	EXPECT_NE(msg.find("syntax_error.lua"), std::string::npos)
		<< "syntax error did not reference the file: " << msg;
}

TEST(FileLoadingTest, RuntimeError_ReturnsErrRunAndPinpointsLine) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	int status = lua.loadAndExecuteScript(dataFile("runtime_error.lua"));

	ASSERT_EQ(status, LUA_ERRRUN);
	ASSERT_FALSE(errors.log().empty());
	EXPECT_EQ(errors.log().front().category, LuaError::Category::Runtime);
	// runtime_error.lua calls error("boom...") on line 4. The traceback must
	// reference both the file and the line.
	const auto& msg = errors.log().front().message;
	EXPECT_NE(msg.find("runtime_error.lua"), std::string::npos)
		<< "runtime error did not reference the file: " << msg;
	EXPECT_NE(msg.find(":4"), std::string::npos)
		<< "runtime error did not reference line 4: " << msg;
}

TEST(FileLoadingTest, NonAsciiPath_LoadsAndExecutes) {
	// Regression guard for the read-via-ifstream rewrite of Registry::loadFile.
	// Prior to it, paths containing characters outside the system's active
	// code page failed to open on Windows (luaL_loadfile -> fopen sees only
	// ACP-encoded paths), even when the file existed. We synthesize a path
	// with diacritics + CJK and confirm the loader still opens it.
	// \uXXXX escapes (not raw bytes) so this compiles to the same UTF-8
	// sequence regardless of the source file's encoding or MSVC's
	// /source-charset setting. The chars are: e-acute (U+00E9) and
	// CJK 'zhong' + 'wen' (U+4E2D, U+6587) — none representable in any
	// single Windows ACP.
	//
	// TODO(c++20-migration): std::filesystem::u8path is deprecated in C++20
	// and removed in C++23. When the library raises its minimum standard,
	// replace with the path(std::u8string_view) constructor that became
	// the canonical UTF-8 entry point in C++20.
	const Lua::File path = std::filesystem::temp_directory_path() /
		std::filesystem::u8path(u8"luacpp_t\u00e9st_\u4e2d\u6587.lua");
	{
		std::ofstream out(path, std::ios::binary);
		ASSERT_TRUE(out.is_open()) << "ifstream could not create the fixture — "
			"if this fails, the test premise (Windows wide-API path support) "
			"is broken on this platform";
		out << "x = 7\n";
	}

	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	const int status = lua.loadAndExecuteScript(path);

	std::error_code rmErr;
	std::filesystem::remove(path, rmErr);  // best-effort cleanup; ignore failure

	ASSERT_EQ(status, LUA_OK)
		<< "non-ASCII path failed to load: "
		<< (errors.log().empty() ? std::string("<no error message>") : errors.log().front().message);
	EXPECT_EQ(lua.readVariable<int>("x"), 7);
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
