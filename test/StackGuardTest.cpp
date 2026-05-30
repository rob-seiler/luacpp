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

TEST(StackGuardTest, leanRestoreCleansUpSilently) {
	// Plain StackGuard does pure top-restore without any debug check —
	// intermediate pushes are silently cleaned up at scope exit. Useful
	// for paths whose intermediate stack state is unpredictable (e.g.
	// popErrorFromStack wrapping luaL_tolstring).
	State lua(State::LibNone);
	lua.pushToStack(1);
	{
		StackGuard guard(lua.getState()); // target = 0
		lua.pushToStack(2);                // untracked
		lua.pushToStack(3);                // also untracked
	}
	EXPECT_EQ(lua.getStackSize(), 0);
}

TEST(AssertionStackGuardTest, tolerateImbalanceAllowsUntrackedIntermediatePushes) {
	// AssertionStackGuard adds a debug-only check for stack imbalance.
	// tolerateImbalance() suppresses the check when the imbalance is
	// deliberate (the only state in which an unsuppressed check would
	// fire in this test).
	State lua(State::LibNone);
	lua.pushToStack(1);
	{
		AssertionStackGuard guard(lua.getState());
		guard.tolerateImbalance();
		lua.pushToStack(2);
		lua.pushToStack(3);
	}
	EXPECT_EQ(lua.getStackSize(), 0)
	    << "top-restore cleans up intermediate pushes when imbalance is tolerated";
}

TEST(AssertionStackGuardTest, balancedScopePassesCheck) {
	// Without tolerateImbalance, AssertionStackGuard only passes when the
	// intermediate stack state nets out — equivalent to count-based discipline.
	State lua(State::LibNone);
	lua.pushToStack(1);
	{
		AssertionStackGuard guard(lua.getState());
		lua.pushToStack(2);
		lua.popStack(1);  // balanced: pushed 1, popped 1
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
