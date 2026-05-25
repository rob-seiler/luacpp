#include <gtest/gtest.h>

#include <luacpp/ErrorHandling.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace Lua {
namespace {

LuaError makeError(LuaError::Category cat = LuaError::Category::Runtime,
                   int status = 2,
                   std::string message = "boom") {
	return LuaError{cat, status, std::move(message), {}};
}

TEST(ErrorHandlingTest, nullHandlerDoesNothing) {
	NullHandler h;
	// Just verifying it doesn't throw or otherwise misbehave.
	h(makeError());
	SUCCEED();
}

TEST(ErrorHandlingTest, logDecoratorAppendsAndDelegates) {
	auto inner = std::make_unique<LogDecorator>(std::make_unique<NullHandler>());
	LogDecorator* innerPtr = inner.get();

	LogDecorator outer(std::move(inner));
	outer(makeError(LuaError::Category::Runtime, 2, "first"));
	outer(makeError(LuaError::Category::Load,    3, "second"));

	ASSERT_EQ(outer.log().size(), 2u);
	EXPECT_EQ(outer.log()[0].message, "first");
	EXPECT_EQ(outer.log()[1].message, "second");

	// Inner ran too — outer delegates.
	ASSERT_EQ(innerPtr->log().size(), 2u);
}

TEST(ErrorHandlingTest, logDecoratorClear) {
	LogDecorator log(std::make_unique<NullHandler>());
	log(makeError());
	ASSERT_EQ(log.log().size(), 1u);
	log.clear();
	EXPECT_TRUE(log.log().empty());
}

TEST(ErrorHandlingTest, captureDecoratorKeepsLastOnly) {
	CaptureDecorator cap(std::make_unique<NullHandler>());
	EXPECT_FALSE(cap.lastError().has_value());

	cap(makeError(LuaError::Category::Runtime, 2, "first"));
	ASSERT_TRUE(cap.lastError().has_value());
	EXPECT_EQ(cap.lastError()->message, "first");

	cap(makeError(LuaError::Category::Runtime, 2, "second"));
	EXPECT_EQ(cap.lastError()->message, "second");

	cap.reset();
	EXPECT_FALSE(cap.lastError().has_value());
}

TEST(ErrorHandlingTest, filterDecoratorRejectsOnFalsePredicate) {
	auto log = std::make_unique<LogDecorator>(std::make_unique<NullHandler>());
	LogDecorator* logPtr = log.get();

	FilterDecorator filter(
	    [](const LuaError& e) { return e.category == LuaError::Category::Load; },
	    std::move(log));

	filter(makeError(LuaError::Category::Runtime)); // dropped
	filter(makeError(LuaError::Category::Load));    // passes

	ASSERT_EQ(logPtr->log().size(), 1u);
	EXPECT_EQ(logPtr->log()[0].category, LuaError::Category::Load);
}

TEST(ErrorHandlingTest, throwDecoratorRunsInnerThenThrows) {
	auto log = std::make_unique<LogDecorator>(std::make_unique<NullHandler>());
	LogDecorator* logPtr = log.get();

	ThrowDecorator thrower(std::move(log));

	try {
		thrower(makeError(LuaError::Category::Runtime, 2, "boom"));
		FAIL() << "ThrowDecorator should have thrown";
	} catch (const LuaException& e) {
		EXPECT_STREQ(e.what(), "boom");
		EXPECT_EQ(e.error().status, 2);
	}

	// Inner ran BEFORE the throw, so log captured the error.
	ASSERT_EQ(logPtr->log().size(), 1u);
	EXPECT_EQ(logPtr->log()[0].message, "boom");
}

TEST(ErrorHandlingTest, callbackDecoratorInvokesCallbackThenDelegates) {
	std::string seen;
	auto log = std::make_unique<LogDecorator>(std::make_unique<NullHandler>());
	LogDecorator* logPtr = log.get();

	CallbackDecorator cb(
	    [&seen](const LuaError& e) { seen = e.message; },
	    std::move(log));

	cb(makeError(LuaError::Category::Runtime, 2, "ping"));

	EXPECT_EQ(seen, "ping");
	ASSERT_EQ(logPtr->log().size(), 1u);
}

TEST(ErrorHandlingTest, outerFirstSemanticsAreCompositionOrderIndependent) {
	// Compose Log inside Throw: Throw delegates first → log captures → throws.
	auto innerLog = std::make_unique<LogDecorator>(std::make_unique<NullHandler>());
	LogDecorator* innerLogPtr = innerLog.get();
	ThrowDecorator throwOuter(std::move(innerLog));

	// Compose Throw inside Log: Log runs first → delegates to Throw → throws.
	auto innerThrow = std::make_unique<ThrowDecorator>(std::make_unique<NullHandler>());
	auto outerLog   = std::make_unique<LogDecorator>(std::move(innerThrow));
	LogDecorator*  outerLogPtr = outerLog.get();

	EXPECT_THROW(throwOuter(makeError()), LuaException);
	EXPECT_EQ(innerLogPtr->log().size(), 1u);

	EXPECT_THROW((*outerLog)(makeError()), LuaException);
	EXPECT_EQ(outerLogPtr->log().size(), 1u);
}

TEST(ErrorHandlingTest, decoratorRejectsNullInner) {
	EXPECT_THROW(LogDecorator(nullptr),     std::invalid_argument);
	EXPECT_THROW(CaptureDecorator(nullptr), std::invalid_argument);
	EXPECT_THROW(ThrowDecorator(nullptr),   std::invalid_argument);
}

TEST(ErrorHandlingTest, filterRejectsNullPredicate) {
	EXPECT_THROW(
	    FilterDecorator(nullptr, std::make_unique<NullHandler>()),
	    std::invalid_argument);
}

TEST(ErrorHandlingTest, callbackRejectsNullCallback) {
	EXPECT_THROW(
	    CallbackDecorator(nullptr, std::make_unique<NullHandler>()),
	    std::invalid_argument);
}

TEST(ErrorHandlingTest, luaExceptionCarriesFullError) {
	LuaError err{LuaError::Category::Load, 3, "syntax", "myfile.lua"};
	try {
		throw LuaException(err);
	} catch (const LuaException& e) {
		EXPECT_EQ(e.error().category, LuaError::Category::Load);
		EXPECT_EQ(e.error().status, 3);
		EXPECT_EQ(e.error().message, "syntax");
		EXPECT_EQ(e.error().source, "myfile.lua");
		EXPECT_STREQ(e.what(), "syntax");
	}
}

} // namespace
} // namespace Lua
