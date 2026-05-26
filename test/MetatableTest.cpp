#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Metatable.hpp>

#include <stdexcept>
#include <string>

namespace Lua {

template <typename T>
static T readVar(State& s, const char* name) {
    auto v = s.readVariable<T>(name);
    if (!v) throw std::runtime_error(std::string("readVar: '") + name + "' missing or wrong type");
    return *v;
}

struct Vector {
    Vector(float ax = 0, float ay = 0) : x(ax), y(ay) {}
    Vector operator+(const Vector& rhs) const { return Vector(x + rhs.x, y + rhs.y); }
    Vector operator-(const Vector& rhs) const { return Vector(x - rhs.x, y - rhs.y); }
    Vector operator*(const Vector& rhs) const { return Vector(x * rhs.x, y * rhs.y); }
    Vector operator/(const Vector& rhs) const {
        return Vector(rhs.x != 0.0f ? x / rhs.x : 0.0f,
                      rhs.y != 0.0f ? y / rhs.y : 0.0f);
    }
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
    lua.loadAndExecuteScript(src);
    Vector* res = readVar<Vector*>(lua, "result");
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
    lua.loadAndExecuteScript(src);
    IntBox* res = readVar<IntBox*>(lua, "result");
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
    lua.loadAndExecuteScript(subSrc);
    Vector* subRes = readVar<Vector*>(lua, "result");
    ASSERT_NE(subRes, nullptr);
    EXPECT_FLOAT_EQ(subRes->x, 3.0f);
    EXPECT_FLOAT_EQ(subRes->y, 4.0f);

    const char* mulSrc = "v1 = createVector(2,3); v2 = createVector(3,4); result = v1 * v2";
    lua.loadAndExecuteScript(mulSrc);
    Vector* mulRes = readVar<Vector*>(lua, "result");
    ASSERT_NE(mulRes, nullptr);
    EXPECT_FLOAT_EQ(mulRes->x, 6.0f);
    EXPECT_FLOAT_EQ(mulRes->y, 12.0f);

    const char* unmSrc = "v1 = createVector(1,2); result = -v1";
    lua.loadAndExecuteScript(unmSrc);
    Vector* unmRes = readVar<Vector*>(lua, "result");
    ASSERT_NE(unmRes, nullptr);
    EXPECT_FLOAT_EQ(unmRes->x, -1.0f);
    EXPECT_FLOAT_EQ(unmRes->y, -2.0f);

    const char* eqSrc = "v1 = createVector(1,2); v2 = createVector(1,2); result = v1 == v2";
    lua.loadAndExecuteScript(eqSrc);
    EXPECT_TRUE(readVar<bool>(lua, "result"));
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

    lua.installErrorHandler<ThrowDecorator>();
    const char* src = "v1 = createVector(1,2); result = v1 + nil";
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
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
    // (Note: Vector + number ALSO errors here. Vector has an implicit
    // `Vector(float, float)` ctor with defaults, so the cross-type guard in
    // can_apply intentionally disables the T+double / double+T branches to
    // avoid silently constructing a Vector from a scalar. Users who want a
    // real mixed-type operator must mark their ctor `explicit` or define
    // operator+(double) directly — see BindMixedOpTest for examples.)
    lua.installErrorHandler<ThrowDecorator>();
    const char* src = "v1 = createVector(1,2); result = v1 + true";
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
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

    lua.installErrorHandler<ThrowDecorator>();
    const char* src = "v1 = createVector(1,2); result = v1 * 'hello'";
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
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

    lua.installErrorHandler<ThrowDecorator>();
    const char* src = "v1 = createVector(1,2); b1 = createBox(5); result = v1 + b1";
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
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

    // Division by Vector(0,0): operator/ guards against zero divisors and
    // returns 0 per component, so the result is deterministic and free of
    // inf/nan.
    const char* src = "v1 = createVector(10,20); v2 = createVector(0,0); result = v1 / v2";
    lua.loadAndExecuteScript(src);

    Vector* res = readVar<Vector*>(lua, "result");
    ASSERT_NE(res, nullptr);
    EXPECT_FLOAT_EQ(res->x, 0.0f);
    EXPECT_FLOAT_EQ(res->y, 0.0f);
}

} // namespace Lua
