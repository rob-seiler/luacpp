#include <gtest/gtest.h>

#include <luacpp/ErrorHandling.hpp>
#include <luacpp/State.hpp>

#include "TestSupport.hpp" // dataFile

#include <string>

namespace Lua {
namespace {

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

TEST(TracebackParseTest, chunkNameContainingSeparatorParses) {
	// luaL_loadstring names chunks after their source text, so a chunk name
	// can itself contain ": in " — the separator scan must skip past it.
	Traceback tb("stack traceback:\n"
	             "\t[string \"s = ': in '\"]:1: in main chunk");
	const auto frames = tb.asList();
	ASSERT_EQ(frames.size(), 1u);
	EXPECT_EQ(frames[0].source, "[string \"s = ': in '\"]");
	EXPECT_EQ(frames[0].line, 1);
	EXPECT_EQ(frames[0].what, "main chunk");
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

// Shared setup: sandboxed VM with base lib, memory logger attached.
struct TracebackVm {
	State lua{State::LibBase};
	MemoryLogger& mem{lua.diagnostics.installLogger<MemoryLogger>()};

	const LuaMessage& firstMessage() {
		static const LuaMessage none;
		if (mem.entries().empty()) {
			ADD_FAILURE() << "no error was reported";
			return none;
		}
		return mem.entries().front().message;
	}
};

TEST(TracebackTest, disabledByDefaultOmitsTraceback) {
	TracebackVm vm;
	EXPECT_FALSE(vm.lua.diagnostics.tracebackEnabled());

	vm.lua.loadAndExecuteScript("error('boom')");

	EXPECT_FALSE(vm.firstMessage().traceback().has_value());
}

TEST(TracebackTest, enabledCapturesTracebackOnRuntimeError) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);
	EXPECT_TRUE(vm.lua.diagnostics.tracebackEnabled());

	vm.lua.loadAndExecuteScript("error('boom')");

	const auto& msg = vm.firstMessage();
	EXPECT_NE(msg.find("boom"), std::string::npos);
	EXPECT_EQ(msg.line(), 1); // prefix parsing is unaffected by the capture
	const auto tb = msg.traceback();
	ASSERT_TRUE(tb.has_value());
	EXPECT_EQ(tb->text().rfind(TracebackHeader, 0), 0u) << tb->text();
	// full() combines message and stack; raw() stays pure message.
	EXPECT_NE(msg.full().find(TracebackHeader), std::string::npos);
	EXPECT_EQ(msg.raw().find(TracebackHeader), std::string::npos);
}

TEST(TracebackTest, nestedCallsShowCallerFrames) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript(
	    "function inner() error('deep') end\n"
	    "function outer() inner() end\n"
	    "outer()");

	const auto tb = vm.firstMessage().traceback();
	ASSERT_TRUE(tb.has_value());
	// Lua 5.5 names the frame kind ("in global 'inner'", "in local 'f'", ...)
	// where older versions always said "in function" — assert only on the
	// quoted names to stay format-agnostic.
	EXPECT_NE(tb->text().find("'inner'"), std::string::npos) << tb->text();
	EXPECT_NE(tb->text().find("'outer'"), std::string::npos) << tb->text();
}

TEST(TracebackTest, realTracebackParsesIntoFrames) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript(
	    "function inner() error('deep') end\n"
	    "function outer() inner() end\n"
	    "outer()");

	const auto tb = vm.firstMessage().traceback();
	ASSERT_TRUE(tb.has_value());

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
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript("function fail(a, b) error('args') end");
	ASSERT_TRUE(vm.mem.entries().empty());

	// numArgs > 0 exercises the handler-slot computation below func+args.
	vm.lua.executeFunction("fail", 1, 2);

	const auto& msg = vm.firstMessage();
	EXPECT_NE(msg.find("args"), std::string::npos);
	EXPECT_TRUE(msg.traceback().has_value()) << msg.raw();
}

TEST(TracebackTest, executeScriptPathCarriesTraceback) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadScript(1, "error('stored')");
	ASSERT_TRUE(vm.mem.entries().empty());
	vm.lua.executeScript(1);

	const auto& msg = vm.firstMessage();
	EXPECT_NE(msg.find("stored"), std::string::npos);
	EXPECT_TRUE(msg.traceback().has_value()) << msg.raw();
}

TEST(TracebackTest, successPathPreservesMultretResults) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	// Regression test for the handler removal: with the msgh inserted below
	// the chunk, all MULTRET results must land exactly where they would have
	// without it.
	vm.lua.loadAndExecuteScript("return 1, 2, 3");

	EXPECT_TRUE(vm.mem.entries().empty());
	ASSERT_EQ(vm.lua.getStackSize(), 3);
	EXPECT_EQ(vm.lua.getStackValue<int>(-3), 1);
	EXPECT_EQ(vm.lua.getStackValue<int>(-2), 2);
	EXPECT_EQ(vm.lua.getStackValue<int>(-1), 3);
}

TEST(TracebackTest, successPathReturningLeavesBalancedStack) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript("function add(a, b) return a + b end");
	const auto result = vm.lua.executeFunctionReturning<int>("add", 20, 22);

	EXPECT_TRUE(vm.mem.entries().empty());
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(*result, 42);
	EXPECT_EQ(vm.lua.getStackSize(), 0) << "handler slot leaked onto the stack";
}

TEST(TracebackTest, nonStringErrorObjectStringifiesLikeDisabledPath) {
	// The opt-in must be purely additive: error objects stringify through
	// the same luaL_tolstring path with and without tracebacks, so a plain
	// table yields "table: 0x..." in both modes (not a placeholder text).
	TracebackVm vm;
	vm.lua.loadAndExecuteScript("error({code = 1})");
	const auto disabledText = vm.firstMessage().raw();
	EXPECT_EQ(disabledText.rfind("table: ", 0), 0u) << disabledText;

	TracebackVm vm2;
	vm2.lua.diagnostics.setTracebackEnabled(true);
	vm2.lua.loadAndExecuteScript("error({code = 1})");
	const auto& msg = vm2.firstMessage();
	EXPECT_EQ(msg.raw().rfind("table: ", 0), 0u) << msg.raw();
	EXPECT_TRUE(msg.traceback().has_value());
}

TEST(TracebackTest, tostringErrorObjectKeepsCustomMessage) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript(
	    "error(setmetatable({}, {__tostring = function() return 'custom msg' end}))");

	const auto& msg = vm.firstMessage();
	EXPECT_EQ(msg.raw(), "custom msg");
	EXPECT_TRUE(msg.traceback().has_value());
}

TEST(TracebackTest, embeddedNulInErrorMessageSurvives) {
	// The traceback is captured out-of-band, so the error object still goes
	// through the length-aware luaL_tolstring — NULs must not truncate it.
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript("error('a\\0b')");

	const std::string expected("a\0b", 3);
	EXPECT_NE(vm.firstMessage().find(expected), std::string::npos);
	EXPECT_TRUE(vm.firstMessage().traceback().has_value());
}

TEST(TracebackTest, userMessageContainingHeaderIsNotMistakenForTraceback) {
	// Regression: the traceback is no longer sniffed out of the message
	// string, so error text that happens to contain the header phrase
	// (e.g. a rethrown debug.traceback() result) cannot fake one.
	TracebackVm vm;
	vm.lua.loadAndExecuteScript("error('msg\\nstack traceback: fake', 0)");

	const auto& msg = vm.firstMessage();
	EXPECT_FALSE(msg.traceback().has_value());
	EXPECT_NE(msg.text().find("stack traceback: fake"), std::string::npos);
}

TEST(TracebackTest, reDisableRestoresPlainMessage) {
	TracebackVm vm;

	vm.lua.diagnostics.setTracebackEnabled(true);
	vm.lua.loadAndExecuteScript("error('first')");
	vm.lua.diagnostics.setTracebackEnabled(false);
	vm.lua.loadAndExecuteScript("error('second')");

	ASSERT_EQ(vm.mem.entries().size(), 2u);
	EXPECT_TRUE(vm.mem.entries()[0].message.traceback().has_value());
	EXPECT_FALSE(vm.mem.entries()[1].message.traceback().has_value());
}

TEST(TracebackTest, fileScriptTracebackReferencesFile) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript(dataFile("runtime_error.lua"));

	const auto& msg = vm.firstMessage();
	// The file name must appear in the prefix AND inside a traceback frame.
	EXPECT_NE(msg.find("runtime_error.lua"), std::string::npos) << msg.raw();
	const auto tb = msg.traceback();
	ASSERT_TRUE(tb.has_value());
	EXPECT_NE(tb->text().find("runtime_error.lua"), std::string::npos)
	    << "no frame references the file: " << tb->text();
}

TEST(TracebackTest, borrowedWrapperSharesTracebackFlag) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	// A borrowed wrapper around the same VM shares the per-VM context —
	// including the traceback flag and the logger.
	State borrowed(vm.lua.getState());
	EXPECT_TRUE(borrowed.diagnostics.tracebackEnabled());
	borrowed.loadAndExecuteScript("error('via borrowed')");

	EXPECT_TRUE(vm.firstMessage().traceback().has_value());
}

TEST(TracebackTest, messageAccessorsSplitTextAndTraceback) {
	TracebackVm vm;
	vm.lua.diagnostics.setTracebackEnabled(true);

	vm.lua.loadAndExecuteScript("error('boom')");

	const auto& msg = vm.firstMessage();
	EXPECT_EQ(msg.text(), "boom");                                   // pure error text
	ASSERT_TRUE(msg.traceback().has_value());                        // the stack block
	EXPECT_EQ(msg.raw().find(TracebackHeader), std::string::npos);   // raw = message only
	EXPECT_NE(msg.full().find("boom"), std::string::npos);           // full = both
	EXPECT_NE(msg.full().find(TracebackHeader), std::string::npos);
}

} // namespace
} // namespace Lua
