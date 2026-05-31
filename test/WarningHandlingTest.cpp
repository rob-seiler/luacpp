#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/WarningHandling.hpp>

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

TEST(WarningLoggerTest, defaultLoggerWritesToCerrWithLuacppTag) {
	// Mirror of the error path: default-constructed State has a
	// StreamWarningLogger to std::cerr installed. We rdbuf-swap cerr to
	// capture the output and verify the "[lua warning]" tag — Lua 5.5's
	// own default warnfon would print "Lua warning: ..." instead, so
	// the tag also acts as evidence our logger is the one running.
	std::ostringstream captured;
	std::streambuf* orig = std::cerr.rdbuf(captured.rdbuf());

	{
		State lua(State::LibBase);
		lua.loadAndExecuteScript("warn('default-routed')");
	}

	std::cerr.rdbuf(orig);
	const std::string text = captured.str();
	EXPECT_NE(text.find("[lua warning]"),  std::string::npos)
	    << "default logger should tag output so it's distinguishable";
	EXPECT_NE(text.find("default-routed"), std::string::npos);
}

} // namespace
} // namespace Lua
