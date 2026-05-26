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
	lua.loadAndExecuteScript(dataFile("valid.lua"));

	EXPECT_TRUE(errors.log().empty());
	EXPECT_EQ(lua.readVariable<int>("x"), 42);
	EXPECT_EQ(lua.readVariable<std::string>("greeting"), "hello from file");
}

TEST(FileLoadingTest, MissingFile_ReportsLoadErrorWithPath) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	lua.loadAndExecuteScript(Lua::File("does_not_exist_xyz.lua"));

	ASSERT_FALSE(errors.log().empty());
	const auto& err = errors.log().front();
	EXPECT_EQ(err.category, LuaError::Category::Load);
	EXPECT_EQ(err.status, LUA_ERRFILE);
	// Lua's standard error format for ERRFILE is
	//   "cannot open <path>: <reason>"
	// We don't pin the entire message (varies by libc) but the path must be in it.
	EXPECT_NE(err.message.find("does_not_exist_xyz.lua"), std::string::npos)
		<< "error message did not reference the failing path: " << err.message;
}

TEST(FileLoadingTest, SyntaxError_ReportsLoadErrorReferencingFile) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	lua.loadAndExecuteScript(dataFile("syntax_error.lua"));

	ASSERT_FALSE(errors.log().empty());
	const auto& err = errors.log().front();
	EXPECT_EQ(err.category, LuaError::Category::Load);
	EXPECT_EQ(err.status, LUA_ERRSYNTAX);
	// chunkname (= '@path') means the file name appears in the error message
	// — that is the whole point of the file-source overload vs. piping the
	// file's bytes through loadAndExecuteScript(const char*).
	EXPECT_NE(err.message.find("syntax_error.lua"), std::string::npos)
		<< "syntax error did not reference the file: " << err.message;
}

TEST(FileLoadingTest, RuntimeError_ReportsRuntimeErrorPinpointingLine) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	lua.loadAndExecuteScript(dataFile("runtime_error.lua"));

	ASSERT_FALSE(errors.log().empty());
	const auto& err = errors.log().front();
	EXPECT_EQ(err.category, LuaError::Category::Runtime);
	EXPECT_EQ(err.status, LUA_ERRRUN);
	// runtime_error.lua calls error("boom...") on line 4. The traceback must
	// reference both the file and the line.
	EXPECT_NE(err.message.find("runtime_error.lua"), std::string::npos)
		<< "runtime error did not reference the file: " << err.message;
	EXPECT_NE(err.message.find(":4"), std::string::npos)
		<< "runtime error did not reference line 4: " << err.message;
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
		std::filesystem::u8path(u8"luacpp_tést_中文.lua");
	{
		std::ofstream out(path, std::ios::binary);
		ASSERT_TRUE(out.is_open()) << "ifstream could not create the fixture — "
			"if this fails, the test premise (Windows wide-API path support) "
			"is broken on this platform";
		out << "x = 7\n";
	}

	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	lua.loadAndExecuteScript(path);

	std::error_code rmErr;
	std::filesystem::remove(path, rmErr);  // best-effort cleanup; ignore failure

	ASSERT_TRUE(errors.log().empty())
		<< "non-ASCII path failed to load: "
		<< errors.log().front().message;
	EXPECT_EQ(lua.readVariable<int>("x"), 7);
}

TEST(FileLoadingTest, RegistryRoundtrip_LoadFromFileThenExecute) {
	State lua;
	auto& errors = lua.installErrorHandler<LogDecorator>();
	const char* key = "stored_script";

	lua.loadScript(key, dataFile("valid.lua"));
	ASSERT_TRUE(errors.log().empty()) << "loadScript failed unexpectedly";

	lua.executeScript(key);
	ASSERT_TRUE(errors.log().empty()) << "executeScript failed unexpectedly";

	EXPECT_EQ(lua.readVariable<int>("x"), 42);
}

} // namespace Lua
