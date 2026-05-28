#include <gtest/gtest.h>

#include <luacpp/ErrorHandling.hpp>
#include <luacpp/State.hpp>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Lua {
namespace {

LuaError makeError(LuaError::Category cat = LuaError::Category::Runtime,
                   int status = 2,
                   std::string message = "boom") {
	return LuaError{cat, status, std::move(message), {}};
}

// ---------------------------------------------------------------------------
// ErrorLogger implementations
// ---------------------------------------------------------------------------

TEST(ErrorLoggerTest, streamLoggerWritesOneLinePerError) {
	std::ostringstream out;
	StreamLogger logger(out);
	logger.log(makeError(LuaError::Category::Load, 3, "syntax"));
	logger.log(makeError(LuaError::Category::Runtime, 2, "boom"));

	const std::string text = out.str();
	EXPECT_NE(text.find("load"),    std::string::npos);
	EXPECT_NE(text.find("syntax"),  std::string::npos);
	EXPECT_NE(text.find("runtime"), std::string::npos);
	EXPECT_NE(text.find("boom"),    std::string::npos);
}

TEST(ErrorLoggerTest, memoryLoggerCollectsAndClears) {
	MemoryLogger logger;
	EXPECT_TRUE(logger.entries().empty());

	logger.log(makeError(LuaError::Category::Runtime, 2, "first"));
	logger.log(makeError(LuaError::Category::Load,    3, "second"));

	ASSERT_EQ(logger.entries().size(), 2u);
	EXPECT_EQ(logger.entries()[0].message, "first");
	EXPECT_EQ(logger.entries()[1].message, "second");

	logger.clear();
	EXPECT_TRUE(logger.entries().empty());
}

TEST(ErrorLoggerTest, callbackLoggerForwardsToFunction) {
	int calls = 0;
	std::string lastMsg;
	CallbackLogger logger([&](const LuaError& e) {
		++calls;
		lastMsg = e.message;
	});

	logger.log(makeError(LuaError::Category::Runtime, 2, "ping"));
	EXPECT_EQ(calls, 1);
	EXPECT_EQ(lastMsg, "ping");
}

TEST(ErrorLoggerTest, callbackLoggerRejectsNullCallback) {
	EXPECT_THROW(CallbackLogger(nullptr), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// ErrorHandler implementations
// ---------------------------------------------------------------------------

TEST(ErrorHandlerTest, throwHandlerThrowsLuaException) {
	ThrowHandler handler;
	try {
		handler(makeError(LuaError::Category::Runtime, 2, "kaboom"));
		FAIL() << "ThrowHandler must throw";
	} catch (const LuaException& ex) {
		EXPECT_STREQ(ex.what(), "kaboom");
		EXPECT_EQ(ex.error().status, 2);
		EXPECT_EQ(ex.error().category, LuaError::Category::Runtime);
	}
}

TEST(ErrorHandlerTest, callbackHandlerForwardsToFunction) {
	int calls = 0;
	CallbackHandler handler([&](const LuaError& e) { ++calls; (void)e; });
	handler(makeError());
	EXPECT_EQ(calls, 1);
}

TEST(ErrorHandlerTest, callbackHandlerRejectsNullCallback) {
	EXPECT_THROW(CallbackHandler(nullptr), std::invalid_argument);
}

TEST(ErrorHandlerTest, luaExceptionCarriesFullError) {
	LuaError err{LuaError::Category::Load, 3, "syntax", "myfile.lua"};
	try {
		throw LuaException(err);
	} catch (const LuaException& ex) {
		EXPECT_EQ(ex.error().category, LuaError::Category::Load);
		EXPECT_EQ(ex.error().status, 3);
		EXPECT_EQ(ex.error().message, "syntax");
		EXPECT_EQ(ex.error().source, "myfile.lua");
		EXPECT_STREQ(ex.what(), "syntax");
	}
}

// ---------------------------------------------------------------------------
// State slot interaction
// ---------------------------------------------------------------------------

TEST(StateErrorSlotsTest, loggerRunsBeforeHandler) {
	// Outer-first semantics: even when the handler throws, the logger has
	// already received the error.
	State lua(State::LibBase);
	auto& mem = lua.installLogger<MemoryLogger>();
	lua.installErrorHandler<ThrowHandler>();

	EXPECT_THROW(lua.loadAndExecuteScript("error('boom')"), LuaException);

	ASSERT_FALSE(mem.entries().empty());
	EXPECT_NE(mem.entries().front().message.find("boom"), std::string::npos);
}

TEST(StateErrorSlotsTest, setLoggerNullSilencesLogging) {
	State lua(State::LibBase);
	lua.setLogger(nullptr);
	// No throw, no log, no output. The script just fails silently — the
	// caller is opting out of every channel.
	lua.loadAndExecuteScript("error('quiet')");
	SUCCEED();
}

TEST(StateErrorSlotsTest, setErrorHandlerNullDoesNotPreventLogging) {
	State lua(State::LibBase);
	auto& mem = lua.installLogger<MemoryLogger>();
	lua.setErrorHandler(nullptr); // explicit; equals the default

	lua.loadAndExecuteScript("error('observe')");

	ASSERT_FALSE(mem.entries().empty());
	EXPECT_NE(mem.entries().front().message.find("observe"), std::string::npos);
}

TEST(StateErrorSlotsTest, customCallbackHandlerCanInspectAndThrowConditionally) {
	State lua(State::LibBase);
	lua.installLogger<MemoryLogger>(); // capture for assertion below

	lua.installErrorHandler<CallbackHandler>([](const LuaError& e) {
		// Only escalate Load errors; runtime errors stay non-throwing.
		if (e.category == LuaError::Category::Load) {
			throw LuaException(e);
		}
	});

	// Runtime error: callback returns, no throw.
	lua.loadAndExecuteScript("error('runtime')");
	SUCCEED();

	// Load error (syntax): callback throws.
	EXPECT_THROW(lua.loadAndExecuteScript("x ="), LuaException);
}

} // namespace
} // namespace Lua
