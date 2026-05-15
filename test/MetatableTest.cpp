#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Metatable.hpp>

namespace Lua {

struct Vector {
    Vector(float ax = 0, float ay = 0) : x(ax), y(ay) {}
    Vector operator+(const Vector& rhs) const { return Vector(x + rhs.x, y + rhs.y); }
    Vector operator-(const Vector& rhs) const { return Vector(x - rhs.x, y - rhs.y); }
    Vector operator*(const Vector& rhs) const { return Vector(x * rhs.x, y * rhs.y); }
    Vector operator/(const Vector& rhs) const { return Vector(x / rhs.x, y / rhs.y); }
    Vector operator-() const { return Vector(-x, -y); }
    bool operator==(const Vector& rhs) const { return x == rhs.x && y == rhs.y; }
    float x; float y;
};

struct IntBox {
    IntBox(int v = 0) : value(v) {}
    IntBox operator+(const IntBox& rhs) const { return IntBox(value + rhs.value); }
    int value;
};

TEST(MetatableTest, VectorUsage) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });
    const char* src = "v1 = createVector(1,2); v2 = createVector(3,4); result = v1 + v2";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);
    Vector* res = lua.readVariable<Vector*>("result");
    ASSERT_NE(res, nullptr);
    EXPECT_FLOAT_EQ(res->x, 4.0f);
    EXPECT_FLOAT_EQ(res->y, 6.0f);
}

TEST(MetatableTest, IntBoxUsage) {
    State lua(State::LibBase);
    Metatable<IntBox>::registerMetatable(lua);
    lua.registerNativeFunction("createBox", [](lua_State* lvm) -> int {
        State L(lvm);
        int val = static_cast<int>(L.getArgument<int>(1));
        Metatable<IntBox>::create(L, val);
        return 1;
    });
    const char* src = "b1 = createBox(5); b2 = createBox(7); result = b1 + b2";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);
    IntBox* res = lua.readVariable<IntBox*>("result");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->value, 12);
}

TEST(MetatableTest, VectorOtherOps) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });

    const char* subSrc = "v1 = createVector(5,7); v2 = createVector(2,3); result = v1 - v2";
    EXPECT_EQ(lua.loadAndExecuteScript(subSrc), 0);
    Vector* subRes = lua.readVariable<Vector*>("result");
    ASSERT_NE(subRes, nullptr);
    EXPECT_FLOAT_EQ(subRes->x, 3.0f);
    EXPECT_FLOAT_EQ(subRes->y, 4.0f);

    const char* mulSrc = "v1 = createVector(2,3); v2 = createVector(3,4); result = v1 * v2";
    EXPECT_EQ(lua.loadAndExecuteScript(mulSrc), 0);
    Vector* mulRes = lua.readVariable<Vector*>("result");
    ASSERT_NE(mulRes, nullptr);
    EXPECT_FLOAT_EQ(mulRes->x, 6.0f);
    EXPECT_FLOAT_EQ(mulRes->y, 12.0f);

    const char* unmSrc = "v1 = createVector(1,2); result = -v1";
    EXPECT_EQ(lua.loadAndExecuteScript(unmSrc), 0);
    Vector* unmRes = lua.readVariable<Vector*>("result");
    ASSERT_NE(unmRes, nullptr);
    EXPECT_FLOAT_EQ(unmRes->x, -1.0f);
    EXPECT_FLOAT_EQ(unmRes->y, -2.0f);

    const char* eqSrc = "v1 = createVector(1,2); v2 = createVector(1,2); result = v1 == v2";
    EXPECT_EQ(lua.loadAndExecuteScript(eqSrc), 0);
    bool eqRes = lua.readVariable<bool>("result");
    EXPECT_TRUE(eqRes);
}

TEST(MetatableTest, ErrorHandling_NilOperand) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });

    // Test with nil operand - should error, not crash
    const char* src = "v1 = createVector(1,2); result = v1 + nil";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);  // Should fail with error
}

TEST(MetatableTest, ErrorHandling_WrongType) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });

    // Boolean is not convertible to Vector or double — should error.
    // (Note: Vector + 5 IS valid via Vector's implicit float ctor — mixed-type
    // scalar ops are supported when conversion is possible.)
    const char* src = "v1 = createVector(1,2); result = v1 + true";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);  // Should fail with error
}

TEST(MetatableTest, ErrorHandling_StringOperand) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });

    // Test with string operand - should error, not crash
    const char* src = "v1 = createVector(1,2); result = v1 * 'hello'";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);  // Should fail with error
}

TEST(MetatableTest, ErrorHandling_TypeConfusion) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    Metatable<IntBox>::registerMetatable(lua);

    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });

    lua.registerNativeFunction("createBox", [](lua_State* lvm) -> int {
        State L(lvm);
        int val = static_cast<int>(L.getArgument<int>(1));
        Metatable<IntBox>::create(L, val);
        return 1;
    });

    // Test mixing different userdata types - should error, not corrupt memory
    const char* src = "v1 = createVector(1,2); b1 = createBox(5); result = v1 + b1";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);  // Should fail with error due to type mismatch
}

TEST(MetatableTest, DivisionByZero) {
    State lua(State::LibBase);
    Metatable<Vector>::registerMetatable(lua);
    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector>::create(L, x, y);
        return 1;
    });

    // Test division by zero - for floats this produces inf/nan (not ideal but doesn't crash)
    const char* src = "v1 = createVector(10,20); v2 = createVector(0,0); result = v1 / v2";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_EQ(status, 0);  // Floats allow division by zero (produces inf/nan)

    Vector* res = lua.readVariable<Vector*>("result");
    ASSERT_NE(res, nullptr);
    // Result will be inf, which is not ideal but at least doesn't crash
}

} // namespace Lua
