#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/StackGuard.hpp>

#include <stdexcept>

namespace Lua {
namespace {

TEST(StackGuardTest, popsOnScopeExit) {
	State lua(State::LibNone);
	ASSERT_EQ(lua.getStackSize(), 0);
	lua.pushToStack(42);
	ASSERT_EQ(lua.getStackSize(), 1);
	{
		StackGuard guard(lua.getState());
	}
	EXPECT_EQ(lua.getStackSize(), 0);
}

TEST(StackGuardTest, releaseCancelsPop) {
	State lua(State::LibNone);
	lua.pushToStack(42);
	{
		StackGuard guard(lua.getState());
		guard.release();
	}
	EXPECT_EQ(lua.getStackSize(), 1) << "release() must prevent the pop";
	lua.popStack(1);
}

TEST(StackGuardTest, popsMultiple) {
	State lua(State::LibNone);
	lua.pushToStack(1);
	lua.pushToStack(2);
	lua.pushToStack(3);
	{
		StackGuard guard(lua.getState(), 3);
	}
	EXPECT_EQ(lua.getStackSize(), 0);
}

TEST(StackGuardTest, growIncreasesPopCount) {
	State lua(State::LibNone);
	lua.pushToStack(1);
	{
		StackGuard guard(lua.getState()); // governs 1
		lua.pushToStack(2);
		guard.grow();                     // now governs 2
	}
	EXPECT_EQ(lua.getStackSize(), 0);
}

// The whole point: a C++ exception unwinding through the scope still pops.
TEST(StackGuardTest, popsDuringCppExceptionUnwind) {
	State lua(State::LibNone);
	lua.pushToStack(42);
	ASSERT_EQ(lua.getStackSize(), 1);
	try {
		StackGuard guard(lua.getState());
		throw std::runtime_error("boom");
	} catch (const std::runtime_error&) {
		// guard destructor ran during stack unwinding
	}
	EXPECT_EQ(lua.getStackSize(), 0);
}

TEST(StackGuardTest, zeroCountIsNoOp) {
	State lua(State::LibNone);
	lua.pushToStack(7);
	{
		StackGuard guard(lua.getState(), 0);
	}
	EXPECT_EQ(lua.getStackSize(), 1);
	lua.popStack(1);
}

} // namespace
} // namespace Lua
