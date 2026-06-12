#include <gtest/gtest.h>

#include <luacpp/Debug.hpp>
#include <luacpp/State.hpp>
#include <luacpp/WarningHandling.hpp>

#include <lua/lua.hpp>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Lua {
namespace {

TEST(WarningLoggerTest, scriptWarnDeliveredAsSingleMessage) {
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	lua.loadAndExecuteScript("warn('hello from lua')");

	ASSERT_EQ(warnings.entries().size(), 1u);
	EXPECT_EQ(warnings.entries().front(), "hello from lua");
}

TEST(WarningLoggerTest, multiPieceWarningAssembled) {
	// warn(a, b, c) concatenates internally — Lua emits one logical warning.
	// This test pins that the pieces arrive at the logger as one message.
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	lua.loadAndExecuteScript("warn('part1 ', 'part2 ', 'part3')");

	ASSERT_EQ(warnings.entries().size(), 1u);
	EXPECT_EQ(warnings.entries().front(), "part1 part2 part3");
}

TEST(WarningLoggerTest, installedLoggerEnablesWarningsImplicitly) {
	// Lua's warning system is disabled by default. Installing a logger is the
	// explicit opt-in, so the very first warn() must already be delivered
	// without needing a prior warn("@on").
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	lua.loadAndExecuteScript("warn('first')");

	ASSERT_EQ(warnings.entries().size(), 1u);
	EXPECT_EQ(warnings.entries().front(), "first");
}

TEST(WarningLoggerTest, controlOffSuppressesSubsequentWarnings) {
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	lua.loadAndExecuteScript(R"(
		warn('before')
		warn('@off')
		warn('muted')
	)");

	ASSERT_EQ(warnings.entries().size(), 1u)
	    << "warn('@off') must mute subsequent warnings";
	EXPECT_EQ(warnings.entries().front(), "before");
}

TEST(WarningLoggerTest, controlOnReEnablesAfterOff) {
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	lua.loadAndExecuteScript(R"(
		warn('@off')
		warn('muted')
		warn('@on')
		warn('back')
	)");

	ASSERT_EQ(warnings.entries().size(), 1u);
	EXPECT_EQ(warnings.entries().front(), "back");
}

TEST(WarningLoggerTest, controlMessagesNeverReachLogger) {
	// '@on' / '@off' and any other leading-'@' message belong to the
	// warning protocol; loggers must never see them as content.
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	lua.loadAndExecuteScript(R"(
		warn('@on')
		warn('@unknownpragma')
		warn('@off')
		warn('@on')
		warn('real')
	)");

	ASSERT_EQ(warnings.entries().size(), 1u);
	EXPECT_EQ(warnings.entries().front(), "real");
}

TEST(WarningLoggerTest, setWarningLoggerNullDisablesWarnings) {
	State lua(State::LibBase);
	{
		auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();
		lua.loadAndExecuteScript("warn('first')");
		ASSERT_EQ(warnings.entries().size(), 1u);
	} // `warnings` ref must not survive setWarningLogger(nullptr) below

	// Detach: subsequent warnings should go nowhere (not even Lua's own
	// stderr default, which we replaced on construction).
	lua.setWarningLogger(nullptr);
	lua.loadAndExecuteScript("warn('dropped')");

	// Reinstall a fresh logger; it must be empty — the warn() emitted while
	// detached is gone, not queued.
	auto& after = lua.installWarningLogger<MemoryWarningLogger>();
	EXPECT_TRUE(after.entries().empty())
	    << "warning emitted while detached must not be replayed to a later logger";
}

TEST(WarningLoggerTest, streamWarningLoggerWritesOneLinePerWarning) {
	std::ostringstream out;
	StreamWarningLogger logger(out);
	logger.log("first");
	logger.log("second");

	const std::string text = out.str();
	EXPECT_NE(text.find("first"),  std::string::npos);
	EXPECT_NE(text.find("second"), std::string::npos);
	EXPECT_NE(text.find("warning"), std::string::npos)
	    << "stream output should be tagged so it's distinguishable from errors";
}

TEST(WarningLoggerTest, callbackWarningLoggerForwardsToFunction) {
	std::string received;
	CallbackWarningLogger logger([&](const std::string& m) { received = m; });
	logger.log("via callback");
	EXPECT_EQ(received, "via callback");
}

TEST(WarningLoggerTest, callbackWarningLoggerRejectsNullCallback) {
	EXPECT_THROW(CallbackWarningLogger{nullptr}, std::invalid_argument);
}

TEST(WarningLoggerTest, defaultLeavesLuaNativeWarnfonActive) {
	// Asymmetry with ErrorLogger by design: the WarningLogger slot starts
	// EMPTY on a freshly constructed State. Lua 5.5's own warnfon stays
	// active and writes "Lua warning: <msg>" to stderr. The point of this
	// test is to lock down that we are NOT auto-installing a luacpp sink —
	// any future change that does would surface as our "[lua warning]" tag
	// appearing in stderr, which we'd want to flag.
	//
	// Note: Lua's lua_writestringerror writes to stderr via the C runtime,
	// bypassing std::cerr's streambuf. Capturing std::cerr therefore only
	// proves that OUR trampoline didn't fire — Lua's native output flows
	// straight to the test runner's stderr. That's enough for this check;
	// a positive assertion on Lua's prefix would need freopen-trickery.
	std::ostringstream captured;
	std::streambuf* orig = std::cerr.rdbuf(captured.rdbuf());

	{
		State lua(State::LibBase);
		lua.loadAndExecuteScript("warn('default-routed')");
	}

	std::cerr.rdbuf(orig);
	const std::string text = captured.str();
	EXPECT_EQ(text.find("[lua warning]"), std::string::npos)
	    << "default WarningLogger slot must be empty — Lua's own warnfon "
	       "should handle warn(), not a luacpp-tagged trampoline";
}

TEST(WarningLoggerTest, borrowedStateRejectsSetWarningLogger) {
	// A State wrapping an externally-owned lua_State does not own the
	// VM-global warning slot. Installing a sink there would clobber the
	// owner's and dangle once the wrapper dies, so it is rejected outright.
	lua_State* raw = luaL_newstate();
	{
		State borrowed(raw);
		EXPECT_THROW(borrowed.setWarningLogger(std::make_unique<MemoryWarningLogger>()),
		             std::logic_error);
		EXPECT_THROW(borrowed.installWarningLogger<MemoryWarningLogger>(),
		             std::logic_error);
	}
	lua_close(raw);
}

TEST(WarningLoggerTest, debugHookWrapperDoesNotClobberOwnersWarningLogger) {
	// Regression: the debug-hook callback builds a transient State around the
	// owner's lua_State. That borrowed wrapper must not touch lua_setwarnf —
	// otherwise it overwrites the owner's sink and leaves a dangling `this`
	// once the hook returns, so the next warn() would hit freed memory.
	State lua(State::LibBase);
	auto& warnings = lua.installWarningLogger<MemoryWarningLogger>();

	int hookCalls = 0;
	lua.registerDebugHook([&hookCalls](State&, const DebugInfo&) {
		++hookCalls;  // a transient borrowed State is constructed for this call
	}, MaskLine, 0);

	// The hook fires per line; the warn() runs after several wrapper
	// construct/destruct cycles. If any of them detached or replaced the
	// owner's sink, this warning would be lost (or crash on a dangling this).
	lua.loadAndExecuteScript(R"(
		local a = 1
		local b = 2
		warn('still routed after hooks')
	)");

	EXPECT_GT(hookCalls, 0) << "debug hook should have fired";
	ASSERT_EQ(warnings.entries().size(), 1u)
	    << "owner's warning sink must survive the borrowed hook wrappers";
	EXPECT_EQ(warnings.entries().front(), "still routed after hooks");
}

} // namespace
} // namespace Lua
