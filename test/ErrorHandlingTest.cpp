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
                   LuaError::Status status = LuaError::Status::RuntimeError,
                   std::string message = "boom") {
	return LuaError{cat, status, std::move(message)};
}

// ---------------------------------------------------------------------------
// LuaMessage prefix parser (best-effort against Lua's standard format)
// ---------------------------------------------------------------------------

TEST(LuaMessageTest, parsesStandardInMemoryChunk) {
	LuaMessage m(std::string(R"([string "x"]:3: attempt to index a nil value)"));
	ASSERT_TRUE(m.source().has_value());
	EXPECT_EQ(*m.source(), R"([string "x"])");
	ASSERT_TRUE(m.line().has_value());
	EXPECT_EQ(*m.line(), 3);
	EXPECT_EQ(m.text(), "attempt to index a nil value");
}

TEST(LuaMessageTest, parsesFilePath) {
	LuaMessage m(std::string("/usr/scripts/run.lua:42: bar"));
	ASSERT_TRUE(m.source().has_value());
	EXPECT_EQ(*m.source(), "/usr/scripts/run.lua");
	EXPECT_EQ(*m.line(), 42);
	EXPECT_EQ(m.text(), "bar");
}

TEST(LuaMessageTest, parsesWindowsPathWithDriveColon) {
	LuaMessage m(std::string("c:\\foo\\script.lua:7: baz"));
	ASSERT_TRUE(m.source().has_value());
	EXPECT_EQ(*m.source(), "c:\\foo\\script.lua");
	EXPECT_EQ(*m.line(), 7);
	EXPECT_EQ(m.text(), "baz");
}

TEST(LuaMessageTest, textPreservesInternalColons) {
	LuaMessage m(std::string("[string \"x\"]:3: foo: bar: baz"));
	EXPECT_EQ(m.text(), "foo: bar: baz");
}

TEST(LuaMessageTest, noPrefixReturnsNullopt) {
	LuaMessage m(std::string("plain error, no prefix"));
	EXPECT_FALSE(m.source().has_value());
	EXPECT_FALSE(m.line().has_value());
	EXPECT_EQ(m.text(), "plain error, no prefix"); // falls back to raw()
}

TEST(LuaMessageTest, malformedLineNumberReturnsNullopt) {
	// Resembles a prefix but the "line" portion isn't an integer.
	LuaMessage m(std::string("source:NaN: msg"));
	EXPECT_FALSE(m.source().has_value());
	EXPECT_FALSE(m.line().has_value());
	EXPECT_EQ(m.text(), "source:NaN: msg");
}

TEST(LuaMessageTest, emptyMessageIsEmpty) {
	LuaMessage m;
	EXPECT_TRUE(m.empty());
	EXPECT_FALSE(m.source().has_value());
	EXPECT_EQ(m.text(), "");
}

// ---------------------------------------------------------------------------
// ErrorLogger implementations
// ---------------------------------------------------------------------------

TEST(ErrorLoggerTest, streamLoggerWritesOneLinePerError) {
	std::ostringstream out;
	StreamLogger logger(out);
	logger.log(makeError(LuaError::Category::Load, LuaError::Status::SyntaxError, "syntax"));
	logger.log(makeError(LuaError::Category::Runtime, LuaError::Status::RuntimeError, "boom"));

	const std::string text = out.str();
	EXPECT_NE(text.find("load"),         std::string::npos);
	EXPECT_NE(text.find("SyntaxError"),  std::string::npos);
	EXPECT_NE(text.find("runtime"),      std::string::npos);
	EXPECT_NE(text.find("RuntimeError"), std::string::npos);
	EXPECT_NE(text.find("boom"),         std::string::npos);
}

TEST(ErrorLoggerTest, streamLoggerLabelsSyntheticByName) {
	std::ostringstream out;
	StreamLogger logger(out);
	logger.log(makeError(LuaError::Category::Runtime,
	                     LuaError::Status::FunctionNotFound, "not a function"));
	const std::string text = out.str();
	EXPECT_NE(text.find("FunctionNotFound"), std::string::npos)
	    << "synthetic-status errors should surface by name, not as raw -1: " << text;
	EXPECT_EQ(text.find("-1"), std::string::npos);
}

// Reviewer regression: synthetic (luacpp-detected) errors and real Lua-raised
// errors are visually distinct in StreamLogger output so log readers can tell
// apart "Lua said this" from "we said this".
TEST(ErrorLoggerTest, streamLoggerOriginLabelDistinguishesLuaFromLuacpp) {
	std::ostringstream out;
	StreamLogger logger(out);

	logger.log(makeError(LuaError::Category::Load,
	                     LuaError::Status::SyntaxError, "x ="));
	logger.log(makeError(LuaError::Category::Runtime,
	                     LuaError::Status::FunctionNotFound, "not a function"));

	const std::string text = out.str();
	EXPECT_NE(text.find("[lua "),    std::string::npos)
	    << "Lua-raised errors should carry the 'lua' origin tag";
	EXPECT_NE(text.find("[luacpp "), std::string::npos)
	    << "luacpp-detected errors should carry the 'luacpp' origin tag";
}

TEST(ErrorLoggerTest, memoryLoggerCollectsAndClears) {
	MemoryLogger logger;
	EXPECT_TRUE(logger.entries().empty());

	logger.log(makeError(LuaError::Category::Runtime, LuaError::Status::RuntimeError, "first"));
	logger.log(makeError(LuaError::Category::Load,    LuaError::Status::SyntaxError,  "second"));

	ASSERT_EQ(logger.entries().size(), 2u);
	EXPECT_EQ(logger.entries()[0].message.raw(), "first");
	EXPECT_EQ(logger.entries()[1].message.raw(), "second");

	logger.clear();
	EXPECT_TRUE(logger.entries().empty());
}

TEST(ErrorLoggerTest, callbackLoggerForwardsToFunction) {
	int calls = 0;
	std::string lastMsg;
	CallbackLogger logger([&](const LuaError& e) {
		++calls;
		lastMsg = e.message.raw();
	});

	logger.log(makeError(LuaError::Category::Runtime, LuaError::Status::RuntimeError, "ping"));
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
		handler(makeError(LuaError::Category::Runtime, LuaError::Status::RuntimeError, "kaboom"));
		FAIL() << "ThrowHandler must throw";
	} catch (const LuaException& ex) {
		EXPECT_STREQ(ex.what(), "kaboom");
		EXPECT_EQ(ex.error().status, LuaError::Status::RuntimeError);
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
	LuaError err{LuaError::Category::Load, LuaError::Status::SyntaxError,
	             std::string("myfile.lua:7: syntax")};
	try {
		throw LuaException(err);
	} catch (const LuaException& ex) {
		EXPECT_EQ(ex.error().category, LuaError::Category::Load);
		EXPECT_EQ(ex.error().status, LuaError::Status::SyntaxError);
		EXPECT_EQ(ex.error().message.raw(), "myfile.lua:7: syntax");
		EXPECT_STREQ(ex.what(), "myfile.lua:7: syntax");
		// Convenience accessors parse the standard Lua prefix.
		ASSERT_TRUE(ex.error().message.source().has_value());
		EXPECT_EQ(*ex.error().message.source(), "myfile.lua");
		ASSERT_TRUE(ex.error().message.line().has_value());
		EXPECT_EQ(*ex.error().message.line(), 7);
		EXPECT_EQ(ex.error().message.text(), "syntax");
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
