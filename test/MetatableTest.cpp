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

} // namespace Lua
