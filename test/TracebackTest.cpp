#include <gtest/gtest.h>

#include <luacpp/ErrorHandling.hpp>
#include <luacpp/State.hpp>

#include <string>

#ifndef LUACPP_TEST_DATA_DIR
#error "LUACPP_TEST_DATA_DIR not defined; CMake target_compile_definitions missing"
#endif

namespace Lua {
namespace {

Lua::File dataFile(const char* name) {
	return Lua::File(LUACPP_TEST_DATA_DIR) / name;
}

constexpr const char* TracebackHeader = "stack traceback:";

// ---------------------------------------------------------------------------
// Traceback::asList() parser (pure string fixtures, no VM)
// ---------------------------------------------------------------------------

TEST(TracebackParseTest, parsesLuaFramesWithSourceLineAndWhat) {
	Traceback tb("stack traceback:\n"
	             "\tscript.lua:12: in function 'update'\n"
	             "\tscript.lua:20: in main chunk");
	const auto frames = tb.asList();
	ASSERT_EQ(frames.size(), 2u);
	EXPECT_EQ(frames[0].raw, "script.lua:12: in function 'update'");
	EXPECT_EQ(frames[0].source, "script.lua");
	EXPECT_EQ(frames[0].line, 12);
	EXPECT_EQ(frames[0].what, "function 'update'");
	EXPECT_EQ(frames[1].source, "script.lua");
	EXPECT_EQ(frames[1].line, 20);
	EXPECT_EQ(frames[1].what, "main chunk");
}

TEST(TracebackParseTest, cFrameHasNoLine) {
	Traceback tb("stack traceback:\n\t[C]: in function 'error'");
	const auto frames = tb.asList();
	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0].source, "[C]");
	EXPECT_FALSE(frames[0].line.has_value());
	EXPECT_EQ(frames[0].what, "function 'error'");
}

TEST(TracebackParseTest, pseudoLinesKeepRawOnly) {
	Traceback tb("stack traceback:\n"
	             "\tscript.lua:3: in function 'f'\n"
	             "\t(...tail calls...)\n"
	             "\t(skipping 10 levels)");
	const auto frames = tb.asList();
	ASSERT_EQ(frames.size(), 3u);
	EXPECT_EQ(frames[1].raw, "(...tail calls...)");
	EXPECT_TRUE(frames[1].source.empty());
	EXPECT_FALSE(frames[1].line.has_value());
	EXPECT_TRUE(frames[1].what.empty());
	EXPECT_EQ(frames[2].raw, "(skipping 10 levels)");
	EXPECT_TRUE(frames[2].what.empty());
}

TEST(TracebackParseTest, windowsPathKeepsDriveColon) {
	Traceback tb("stack traceback:\n\td:\\scripts\\foo.lua:7: in function 'boot'");
	const auto frames = tb.asList();
	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0].source, "d:\\scripts\\foo.lua");
	EXPECT_EQ(frames[0].line, 7);
	EXPECT_EQ(frames[0].what, "function 'boot'");
}

TEST(TracebackParseTest, sourceWithoutLineParses) {
	// luaL_traceback omits ":<line>" when no current line is available.
	Traceback tb("stack traceback:\n\tscript.lua: in function <script.lua:5>");
	const auto frames = tb.asList();
	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0].source, "script.lua");
	EXPECT_FALSE(frames[0].line.has_value());
}

TEST(TracebackParseTest, headerOnlyAndEmptyYieldNoFrames) {
	EXPECT_TRUE(Traceback("stack traceback:").asList().empty());
	Traceback empty;
	EXPECT_TRUE(empty.empty());
	EXPECT_TRUE(empty.asList().empty());
}

// ---------------------------------------------------------------------------
// Opt-in behavior through the State pcall paths
// ---------------------------------------------------------------------------

TEST(TracebackTest, disabledByDefaultOmitsTraceback) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();

	EXPECT_FALSE(lua.diagnostics.tracebackEnabled());
	lua.loadAndExecuteScript("error('boom')");

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	EXPECT_EQ(msg.find(TracebackHeader), std::string::npos)
	    << "traceback appeared without opt-in: " << msg;
	EXPECT_FALSE(msg.traceback().has_value());
}

TEST(TracebackTest, enabledAppendsTracebackToRuntimeError) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);
	EXPECT_TRUE(lua.diagnostics.tracebackEnabled());

	lua.loadAndExecuteScript("error('boom')");

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	EXPECT_NE(msg.find("boom"), std::string::npos);
	EXPECT_NE(msg.find(TracebackHeader), std::string::npos)
	    << "no traceback despite opt-in: " << msg;
	// The "<chunk>:<line>: " prefix stays on line 1 — the LuaMessage contract.
	EXPECT_EQ(msg.line(), 1);
}

TEST(TracebackTest, nestedCallsShowCallerFrames) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript(
	    "function inner() error('deep') end\n"
	    "function outer() inner() end\n"
	    "outer()");

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	// Lua 5.5 names the frame kind ("in global 'inner'", "in local 'f'", ...)
	// where older versions always said "in function" — assert only on the
	// quoted names to stay format-agnostic.
	EXPECT_NE(msg.find(TracebackHeader), std::string::npos) << msg;
	EXPECT_NE(msg.find("'inner'"), std::string::npos) << msg;
	EXPECT_NE(msg.find("'outer'"), std::string::npos) << msg;
}

TEST(TracebackTest, realTracebackParsesIntoFrames) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript(
	    "function inner() error('deep') end\n"
	    "function outer() inner() end\n"
	    "outer()");

	ASSERT_FALSE(mem.entries().empty());
	const auto tb = mem.entries().front().message.traceback();
	ASSERT_TRUE(tb.has_value());
	EXPECT_EQ(tb->text().rfind(TracebackHeader, 0), 0u)
	    << "traceback block must start with the header: " << tb->text();

	// Guards the parser against format drift in Lua: a real traceback must
	// decompose into the frames we expect (error(), inner, outer, main chunk).
	const auto frames = tb->asList();
	ASSERT_GE(frames.size(), 3u);
	bool sawInner = false, sawMain = false;
	for (const auto& f : frames) {
		if (f.what.find("'inner'") != std::string::npos) {
			sawInner = true;
			EXPECT_EQ(f.line, 1) << "inner() errors on chunk line 1: " << f.raw;
			EXPECT_FALSE(f.source.empty());
		}
		if (f.what == "main chunk") sawMain = true;
	}
	EXPECT_TRUE(sawInner) << tb->text();
	EXPECT_TRUE(sawMain) << tb->text();
}

TEST(TracebackTest, executeFunctionPathCarriesTraceback) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript("function fail(a, b) error('args') end");
	ASSERT_TRUE(mem.entries().empty());

	// numArgs > 0 exercises the handler-slot computation below func+args.
	lua.executeFunction("fail", 1, 2);

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	EXPECT_NE(msg.find("args"), std::string::npos);
	EXPECT_NE(msg.find(TracebackHeader), std::string::npos) << msg;
}

TEST(TracebackTest, executeScriptPathCarriesTraceback) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadScript(1, "error('stored')");
	ASSERT_TRUE(mem.entries().empty());
	lua.executeScript(1);

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	EXPECT_NE(msg.find("stored"), std::string::npos);
	EXPECT_NE(msg.find(TracebackHeader), std::string::npos) << msg;
}

TEST(TracebackTest, successPathPreservesMultretResults) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	// Regression test for the handler removal: with the msgh inserted below
	// the chunk, all MULTRET results must land exactly where they would have
	// without it.
	lua.loadAndExecuteScript("return 1, 2, 3");

	EXPECT_TRUE(mem.entries().empty());
	ASSERT_EQ(lua.getStackSize(), 3);
	EXPECT_EQ(lua.getStackValue<int>(-3), 1);
	EXPECT_EQ(lua.getStackValue<int>(-2), 2);
	EXPECT_EQ(lua.getStackValue<int>(-1), 3);
}

TEST(TracebackTest, successPathReturningLeavesBalancedStack) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript("function add(a, b) return a + b end");
	const auto result = lua.executeFunctionReturning<int>("add", 20, 22);

	EXPECT_TRUE(mem.entries().empty());
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 42);
	EXPECT_EQ(lua.getStackSize(), 0) << "handler slot leaked onto the stack";
}

TEST(TracebackTest, nonStringErrorObjectGetsPlaceholderAndTraceback) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript("error({code = 1})");

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	EXPECT_NE(msg.find("(error object is a table value)"), std::string::npos) << msg;
	EXPECT_NE(msg.find(TracebackHeader), std::string::npos) << msg;
}

TEST(TracebackTest, tostringErrorObjectKeepsCustomMessage) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript(
	    "error(setmetatable({}, {__tostring = function() return 'custom msg' end}))");

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	EXPECT_NE(msg.find("custom msg"), std::string::npos) << msg;
	// Documented deviation from lua.c's msghandler: __tostring-convertible
	// error objects get a traceback appended too.
	EXPECT_NE(msg.find(TracebackHeader), std::string::npos) << msg;
}

TEST(TracebackTest, reDisableRestoresPlainMessage) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();

	lua.diagnostics.setTracebackEnabled(true);
	lua.loadAndExecuteScript("error('first')");
	lua.diagnostics.setTracebackEnabled(false);
	lua.loadAndExecuteScript("error('second')");

	ASSERT_EQ(mem.entries().size(), 2u);
	EXPECT_NE(mem.entries()[0].message.find(TracebackHeader), std::string::npos);
	EXPECT_EQ(mem.entries()[1].message.find(TracebackHeader), std::string::npos)
	    << "traceback still present after opt-out: " << mem.entries()[1].message;
}

TEST(TracebackTest, fileScriptTracebackReferencesFile) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript(dataFile("runtime_error.lua"));

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;
	const auto headerPos = msg.find(TracebackHeader);
	ASSERT_NE(headerPos, std::string::npos) << msg;
	// The file name must appear in the prefix AND inside a traceback frame.
	EXPECT_NE(msg.find("runtime_error.lua"), std::string::npos) << msg;
	EXPECT_NE(msg.find("runtime_error.lua", headerPos), std::string::npos)
	    << "no frame references the file: " << msg;
}

TEST(TracebackTest, borrowedWrapperSharesTracebackFlag) {
	State owner(State::LibBase);
	auto& mem = owner.diagnostics.installLogger<MemoryLogger>();
	owner.diagnostics.setTracebackEnabled(true);

	// A borrowed wrapper around the same VM shares the per-VM context —
	// including the traceback flag and the logger.
	State borrowed(owner.getState());
	EXPECT_TRUE(borrowed.diagnostics.tracebackEnabled());
	borrowed.loadAndExecuteScript("error('via borrowed')");

	ASSERT_FALSE(mem.entries().empty());
	EXPECT_NE(mem.entries().front().message.find(TracebackHeader), std::string::npos);
}

TEST(TracebackTest, messageAccessorsSplitTextAndTraceback) {
	State lua(State::LibBase);
	auto& mem = lua.diagnostics.installLogger<MemoryLogger>();
	lua.diagnostics.setTracebackEnabled(true);

	lua.loadAndExecuteScript("error('boom')");

	ASSERT_FALSE(mem.entries().empty());
	const auto& msg = mem.entries().front().message;

	// text(): error text only — no prefix, no traceback block.
	EXPECT_EQ(msg.text(), "boom");
	// traceback(): the block, verbatim, starting at the header.
	const auto tb = msg.traceback();
	ASSERT_TRUE(tb.has_value());
	EXPECT_EQ(tb->text().rfind(TracebackHeader, 0), 0u);
	// raw(): both parts combined.
	EXPECT_NE(msg.raw().find("boom"), std::string::npos);
	EXPECT_NE(msg.raw().find(TracebackHeader), std::string::npos);
}

} // namespace
} // namespace Lua
