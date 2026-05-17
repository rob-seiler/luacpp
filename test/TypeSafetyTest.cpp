#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Metatable.hpp>
#include <luacpp/Basics.hpp>
#include <lua/lua.hpp>

namespace Lua {

// ============================================================================
// Test Types
// ============================================================================
// Wrapped in an anonymous namespace so these test-local types do not collide
// with same-named types in other test TUs (e.g. BindTest.cpp also defines a
// `Point` and `Counter`). Without this, both would resolve to `Lua::Point` /
// `Lua::Counter` and the differing definitions would violate the ODR.
namespace {

struct Point {
    Point(float x_, float y_) : x(x_), y(y_) {}
    float x, y;
};

struct Counter {
    explicit Counter(int value_) : value(value_) {}
    int value;
};

} // namespace

// ============================================================================
// Type Safety Tests (Issue #2 from code review)
// ============================================================================
// These tests verify that Basics::checkUserData properly validates types
// and throws Lua errors (via luaL_checkudata) rather than returning nullptr
// ============================================================================

TEST(TypeSafetyTest, CheckUserData_CorrectType) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    // Create a valid Point
    const char* src = "p = Point(3, 4)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    // Should retrieve successfully
    Point* p = lua.readVariable<Point*>("p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 3.0f);
    EXPECT_FLOAT_EQ(p->y, 4.0f);
}

TEST(TypeSafetyTest, CheckUserData_WrongType_ThrowsError) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Counter>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");
    lua.bindConstructor<Counter, int>("Counter");

    // Register a function that tries to use Counter as Point
    lua.registerNativeFunction("testWrongType", [](lua_State* lvm) -> int {
        State L(lvm);
        // This should throw a Lua error (luaL_checkudata behavior)
        // It will NEVER return nullptr - it longjmps on mismatch
        Point* p = L.getArgument<Point*>(1);
        L.pushToStack(p->x);
        return 1;
    });

    // Pass Counter where Point is expected - should error
    const char* src = "c = Counter(42); result = testWrongType(c)";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0); // Should fail with type error from luaL_checkudata
}

TEST(TypeSafetyTest, CheckUserData_NilValue_ThrowsError) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    lua.registerNativeFunction("testNil", [](lua_State* lvm) -> int {
        State L(lvm);
        // luaL_checkudata throws Lua error on nil, never returns nullptr
        Point* p = L.getArgument<Point*>(1);
        L.pushToStack(p->x);
        return 1;
    });

    // Pass nil - should error
    const char* src = "result = testNil(nil)";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0); // Should fail
}

TEST(TypeSafetyTest, CheckUserData_NumberValue_ThrowsError) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    lua.registerNativeFunction("testNumber", [](lua_State* lvm) -> int {
        State L(lvm);
        // luaL_checkudata throws Lua error on wrong type
        Point* p = L.getArgument<Point*>(1);
        L.pushToStack(p->x);
        return 1;
    });

    // Pass number - should error
    const char* src = "result = testNumber(42)";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0); // Should fail
}

TEST(TypeSafetyTest, CheckUserData_StringValue_ThrowsError) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    lua.registerNativeFunction("testString", [](lua_State* lvm) -> int {
        State L(lvm);
        Point* p = L.getArgument<Point*>(1);
        L.pushToStack(p->x);
        return 1;
    });

    // Pass string - should error
    const char* src = "result = testString('hello')";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0); // Should fail
}

TEST(TypeSafetyTest, CheckUserData_MultipleTypes) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Counter>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");
    lua.bindConstructor<Counter, int>("Counter");

    // Function that accepts Point
    lua.registerNativeFunction("getX", [](lua_State* lvm) -> int {
        State L(lvm);
        Point* p = L.getArgument<Point*>(1);
        L.pushToStack(p->x);
        return 1;
    });

    // Function that accepts Counter
    lua.registerNativeFunction("getValue", [](lua_State* lvm) -> int {
        State L(lvm);
        Counter* c = L.getArgument<Counter*>(1);
        L.pushToStack(c->value);
        return 1;
    });

    // Correct usage
    const char* src1 = "p = Point(3, 4); x = getX(p)";
    EXPECT_EQ(lua.loadAndExecuteScript(src1), 0);
    float x = lua.readVariable<float>("x");
    EXPECT_FLOAT_EQ(x, 3.0f);

    const char* src2 = "c = Counter(42); v = getValue(c)";
    EXPECT_EQ(lua.loadAndExecuteScript(src2), 0);
    int v = lua.readVariable<int>("v");
    EXPECT_EQ(v, 42);

    // Wrong type usage
    const char* src3 = "x = getX(c)"; // Pass Counter to Point function
    EXPECT_NE(lua.loadAndExecuteScript(src3), 0);

    const char* src4 = "v = getValue(p)"; // Pass Point to Counter function
    EXPECT_NE(lua.loadAndExecuteScript(src4), 0);
}

// ============================================================================
// Direct Basics::checkUserData Tests
// ============================================================================

TEST(TypeSafetyTest, Basics_CheckUserData_Behavior) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    // Create a Point and push to stack
    const char* src = "p = Point(5, 6); return p";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    lua_State* L = lua.getState();
    lua_getglobal(L, "p");

    // Direct check with correct type name should work
    const char* correctName = Metatable<Point>::metatableName();
    void* result = Basics::checkUserData(L, -1, correctName);
    EXPECT_NE(result, nullptr);

    Point* p = static_cast<Point*>(result);
    EXPECT_FLOAT_EQ(p->x, 5.0f);
    EXPECT_FLOAT_EQ(p->y, 6.0f);

    lua_pop(L, 1);
}

TEST(TypeSafetyTest, Basics_AsUserData_vs_CheckUserData) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "p = Point(7, 8); n = 42";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    lua_State* L = lua.getState();

    // Test with userdata - both should work
    lua_getglobal(L, "p");
    void* as1 = Basics::asUserData(L, -1);
    EXPECT_NE(as1, nullptr); // asUserData returns the userdata

    void* check1 = Basics::checkUserData(L, -1, Metatable<Point>::metatableName());
    EXPECT_NE(check1, nullptr); // checkUserData validates and returns
    EXPECT_EQ(as1, check1); // Should be same pointer

    lua_pop(L, 1);

    // Test with non-userdata (number)
    lua_getglobal(L, "n");
    void* as2 = Basics::asUserData(L, -1);
    EXPECT_EQ(as2, nullptr); // asUserData returns nullptr for non-userdata

    // checkUserData would throw here, so we can't test it directly
    // (it would longjmp out of the test)

    lua_pop(L, 1);
}

// ============================================================================
// Documentation Tests
// ============================================================================
// These tests serve as documentation for the expected behavior

TEST(TypeSafetyTest, Documentation_CheckUserDataNeverReturnsNull) {
    // DOCUMENTATION: Basics::checkUserData uses luaL_checkudata which:
    // 1. Validates the type matches the metatable name
    // 2. On SUCCESS: returns a valid pointer (never nullptr)
    // 3. On FAILURE: throws a Lua error via longjmp (never returns)
    //
    // Therefore:
    // - You NEVER need to check for nullptr after checkUserData
    // - Type mismatches will be caught as Lua errors
    // - The error will propagate through Lua's error handling

    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    lua.registerNativeFunction("safeFunction", [](lua_State* lvm) -> int {
        State L(lvm);
        Point* p = L.getArgument<Point*>(1); // Uses checkUserData internally
        // No need for: if (p == nullptr) { ... }
        // If we reach here, p is ALWAYS valid
        L.pushToStack(p->x + p->y);
        return 1;
    });

    const char* src = "p = Point(3, 4); result = safeFunction(p)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);
    float result = lua.readVariable<float>("result");
    EXPECT_FLOAT_EQ(result, 7.0f);
}

TEST(TypeSafetyTest, Documentation_AsUserDataCanReturnNull) {
    // DOCUMENTATION: Basics::asUserData uses lua_touserdata which:
    // 1. Returns the userdata pointer if value is userdata
    // 2. Returns nullptr if value is NOT userdata (no type check)
    // 3. Never throws errors
    //
    // Use asUserData when:
    // - You want to check IF something is userdata
    // - You'll handle nullptr yourself
    // - You don't need type validation

    State lua(State::LibBase);

    lua.registerNativeFunction("tryGetUserdata", [](lua_State* lvm) -> int {
        State L(lvm);
        void* ud = Basics::asUserData(lvm, 1);
        if (ud == nullptr) {
            L.pushToStack(false); // Not userdata
        } else {
            L.pushToStack(true); // Is userdata (but we don't know what type!)
        }
        return 1;
    });

    const char* src = R"(
        isNum = tryGetUserdata(42)
        isStr = tryGetUserdata("hello")
        isNil = tryGetUserdata(nil)
    )";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    EXPECT_FALSE(lua.readVariable<bool>("isNum"));
    EXPECT_FALSE(lua.readVariable<bool>("isStr"));
    EXPECT_FALSE(lua.readVariable<bool>("isNil"));
}

} // namespace Lua
