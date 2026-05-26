#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Metatable.hpp>

#include <cmath>
#include <stdexcept>
#include <string>

namespace Lua {

// Test-local helper: unwrap the optional<T> returned by readVariable<T> or
// throw if the variable is missing / has the wrong type. Throwing fails the
// test loudly instead of letting a default-T value silently propagate.
namespace {
template <typename T>
T readVar(State& s, const char* name) {
    auto v = s.readVariable<T>(name);
    if (!v) throw std::runtime_error(std::string("readVar: '") + name + "' missing or wrong type");
    return *v;
}
} // namespace


// ============================================================================
// Test Types
// ============================================================================
// Wrapped in an anonymous namespace so these test-local types do not collide
// with same-named types in other test TUs (e.g. TypeSafetyTest.cpp also
// defines `Point` and `Counter`). Without this, both would resolve to
// `Lua::Point` / `Lua::Counter` and the differing definitions would violate
// the ODR.
namespace {

struct Point {
    Point(float x_, float y_) : x(x_), y(y_) {}
    Point operator+(const Point& rhs) const { return Point(x + rhs.x, y + rhs.y); }
    float x, y;
};

struct Resource {
    explicit Resource(int id_) : id(id_), moveCount(0) {}

    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;

    Resource(Resource&& other) noexcept : id(other.id), moveCount(other.moveCount + 1) {
        other.id = -1;
    }

    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            id = other.id;
            moveCount = other.moveCount + 1;
            other.id = -1;
        }
        return *this;
    }

    int id;
    int moveCount;
};

struct Line {
    Line(Point start_, Point end_) : start(start_), end(end_) {}
    Point start;
    Point end;
};

struct Counter {
    explicit Counter(int value_) : value(value_) {}
    Counter operator+(const Counter& rhs) const { return Counter(value + rhs.value); }
    int value;
};

struct Vec {
    float x, y;
    Vec(float ax = 0, float ay = 0) : x(ax), y(ay) {}
    Vec operator+(const Vec& rhs) const { return Vec(x + rhs.x, y + rhs.y); }

    float length() const { return std::sqrt(x * x + y * y); }
    Vec scaled(float s) const { return Vec(x * s, y * s); }
    float dot(const Vec& o) const { return x * o.x + y * o.y; }
    void reset() { x = 0; y = 0; }
    int count(int a, int b, int c) const { return a + b + c; }
};

struct Other {
    int value;
    explicit Other(int v = 0) : value(v) {}
    int doubled() const { return value * 2; }
};

struct Stringable {
    int value;
    explicit Stringable(int v) : value(v) {}
    std::string toString() const { return "Stringable(" + std::to_string(value) + ")"; }
};

struct Comparable {
    int value;
    explicit Comparable(int v) : value(v) {}
    bool operator<(const Comparable& rhs) const { return value < rhs.value; }
    bool operator<=(const Comparable& rhs) const { return value <= rhs.value; }
    bool operator==(const Comparable& rhs) const { return value == rhs.value; }
};

struct Particle {
    float x, y;
    int health;
    std::string name;
    Vec velocity;

    Particle(float ax, float ay, int h)
        : x(ax), y(ay), health(h), name("particle"), velocity(0, 0) {}
};

} // namespace (test types)

// ============================================================================
// Constructor Binding Tests
// ============================================================================

TEST(BindConstructorTest, BasicConstructor) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "p = Point(3.5, 4.5)";
    lua.loadAndExecuteScript(src);

    Point* p = readVar<Point*>(lua,"p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 3.5f);
    EXPECT_FLOAT_EQ(p->y, 4.5f);
}

TEST(BindConstructorTest, SingleArgumentConstructor) {
    State lua(State::LibBase);
    Metatable<Counter>::registerMetatable(lua);
    lua.bindConstructor<Counter, int>("Counter");

    const char* src = "c = Counter(42)";
    lua.loadAndExecuteScript(src);

    Counter* c = readVar<Counter*>(lua,"c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->value, 42);
}

TEST(BindConstructorTest, ConstructorWithOperators) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "p1 = Point(1, 2); p2 = Point(3, 4); result = p1 + p2";
    lua.loadAndExecuteScript(src);

    Point* result = readVar<Point*>(lua,"result");
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->x, 4.0f);
    EXPECT_FLOAT_EQ(result->y, 6.0f);
}

TEST(BindConstructorTest, MultipleConstructors) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Counter>::registerMetatable(lua);

    lua.bindConstructor<Point, float, float>("Point");
    lua.bindConstructor<Counter, int>("Counter");

    const char* src = "p = Point(1.5, 2.5); c = Counter(42)";
    lua.loadAndExecuteScript(src);

    Point* p = readVar<Point*>(lua,"p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 1.5f);
    EXPECT_FLOAT_EQ(p->y, 2.5f);

    Counter* c = readVar<Counter*>(lua,"c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->value, 42);
}

TEST(BindConstructorTest, ConstructorWithUserdataArgs) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Line>::registerMetatable(lua);

    lua.bindConstructor<Point, float, float>("Point");
    lua.bindConstructor<Line, Point, Point>("Line");

    const char* src = "start = Point(0, 0); finish = Point(10, 20); line = Line(start, finish)";
    lua.loadAndExecuteScript(src);

    Line* line = readVar<Line*>(lua,"line");
    ASSERT_NE(line, nullptr);
    EXPECT_FLOAT_EQ(line->start.x, 0.0f);
    EXPECT_FLOAT_EQ(line->start.y, 0.0f);
    EXPECT_FLOAT_EQ(line->end.x, 10.0f);
    EXPECT_FLOAT_EQ(line->end.y, 20.0f);
}

TEST(BindConstructorTest, ConstructorWithInlineUserdataArgs) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Line>::registerMetatable(lua);

    lua.bindConstructor<Point, float, float>("Point");
    lua.bindConstructor<Line, Point, Point>("Line");

    const char* src = "line = Line(Point(1, 2), Point(3, 4))";
    lua.loadAndExecuteScript(src);

    Line* line = readVar<Line*>(lua,"line");
    ASSERT_NE(line, nullptr);
    EXPECT_FLOAT_EQ(line->start.x, 1.0f);
    EXPECT_FLOAT_EQ(line->start.y, 2.0f);
    EXPECT_FLOAT_EQ(line->end.x, 3.0f);
    EXPECT_FLOAT_EQ(line->end.y, 4.0f);
}

TEST(BindConstructorTest, NonCopyableType) {
    State lua(State::LibBase);
    Metatable<Resource>::registerMetatable(lua);
    lua.bindConstructor<Resource, int>("Resource");

    lua.registerNativeFunction("getId", [](lua_State* lvm) -> int {
        State L(lvm);
        Resource* res = L.getArgument<Resource*>(1);
        L.pushToStack(res->id);
        return 1;
    });

    const char* src = "r = Resource(123); id = getId(r)";
    lua.loadAndExecuteScript(src);

    int id = readVar<int>(lua,"id");
    EXPECT_EQ(id, 123);

    Resource* r = readVar<Resource*>(lua,"r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->id, 123);
    EXPECT_EQ(r->moveCount, 0); // constructed in place, never moved
}

TEST(BindConstructorTest, MultipleNonCopyableInstances) {
    State lua(State::LibBase);
    Metatable<Resource>::registerMetatable(lua);
    lua.bindConstructor<Resource, int>("Resource");

    const char* src = R"(
        r1 = Resource(100)
        r2 = Resource(200)
        r3 = Resource(300)
    )";
    lua.loadAndExecuteScript(src);

    Resource* r1 = readVar<Resource*>(lua,"r1");
    Resource* r2 = readVar<Resource*>(lua,"r2");
    Resource* r3 = readVar<Resource*>(lua,"r3");

    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_NE(r3, nullptr);

    EXPECT_EQ(r1->id, 100);
    EXPECT_EQ(r2->id, 200);
    EXPECT_EQ(r3->id, 300);

    EXPECT_EQ(r1->moveCount, 0);
    EXPECT_EQ(r2->moveCount, 0);
    EXPECT_EQ(r3->moveCount, 0);
}

TEST(BindConstructorTest, ConstructorCallSemantics) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = R"(
        p1 = Point(1, 2)
        local p2 = Point(3, 4)
        function makePoint()
            return Point(5, 6)
        end
        p3 = makePoint()
    )";
    lua.loadAndExecuteScript(src);

    Point* p1 = readVar<Point*>(lua,"p1");
    ASSERT_NE(p1, nullptr);
    EXPECT_FLOAT_EQ(p1->x, 1.0f);
    EXPECT_FLOAT_EQ(p1->y, 2.0f);

    Point* p3 = readVar<Point*>(lua,"p3");
    ASSERT_NE(p3, nullptr);
    EXPECT_FLOAT_EQ(p3->x, 5.0f);
    EXPECT_FLOAT_EQ(p3->y, 6.0f);
}

TEST(BindConstructorTest, ComplexExpression) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "result = Point(1, 2) + Point(3, 4) + Point(5, 6)";
    lua.loadAndExecuteScript(src);

    Point* result = readVar<Point*>(lua,"result");
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->x, 9.0f);
    EXPECT_FLOAT_EQ(result->y, 12.0f);
}

TEST(BindConstructorTest, ConstructorInTable) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = R"(
        points = {
            Point(1, 2),
            Point(3, 4),
            Point(5, 6)
        }
    )";
    lua.loadAndExecuteScript(src);

    lua.loadAndExecuteScript("first = points[1]");
    Point* first = readVar<Point*>(lua,"first");
    ASSERT_NE(first, nullptr);
    EXPECT_FLOAT_EQ(first->x, 1.0f);
    EXPECT_FLOAT_EQ(first->y, 2.0f);
}

TEST(BindConstructorTest, ConstructorInLoop) {
    State lua(State::LibBase);
    Metatable<Counter>::registerMetatable(lua);
    lua.bindConstructor<Counter, int>("Counter");

    const char* src = R"(
        sum = Counter(0)
        for i = 1, 5 do
            sum = sum + Counter(i)
        end
    )";
    lua.loadAndExecuteScript(src);

    Counter* sum = readVar<Counter*>(lua,"sum");
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->value, 15);
}

TEST(BindConstructorTest, ErrorHandling_WrongArgCount) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src1 = "p = Point(1)";
    lua.loadAndExecuteScript(src1);

    Point* p1 = readVar<Point*>(lua,"p");
    ASSERT_NE(p1, nullptr);
    EXPECT_FLOAT_EQ(p1->x, 1.0f);
    EXPECT_FLOAT_EQ(p1->y, 0.0f);

    const char* src2 = "p = Point(1, 2, 3, 4)";
    lua.loadAndExecuteScript(src2);

    Point* p2 = readVar<Point*>(lua,"p");
    ASSERT_NE(p2, nullptr);
    EXPECT_FLOAT_EQ(p2->x, 1.0f);
    EXPECT_FLOAT_EQ(p2->y, 2.0f);
}

TEST(BindConstructorTest, ErrorHandling_WrongUserdataType) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Line>::registerMetatable(lua);
    Metatable<Counter>::registerMetatable(lua);

    lua.bindConstructor<Point, float, float>("Point");
    lua.bindConstructor<Line, Point, Point>("Line");
    lua.bindConstructor<Counter, int>("Counter");

    const char* src = "c = Counter(5); line = Line(c, c)";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
}

TEST(BindConstructorTest, ErrorHandling_NilArgument) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "p = Point(nil, 2)";
    lua.loadAndExecuteScript(src);
    // Just verify it doesn't crash; behavior depends on getArgument implementation
}

TEST(BindConstructorTest, ZeroSizedType) {
    struct Empty {
        Empty() = default;
    };

    State lua(State::LibBase);
    Metatable<Empty>::registerMetatable(lua);
    lua.bindConstructor<Empty>("Empty");

    const char* src = "e = Empty()";
    lua.loadAndExecuteScript(src);

    Empty* e = readVar<Empty*>(lua,"e");
    EXPECT_NE(e, nullptr);
}

TEST(BindConstructorTest, LargeType) {
    struct Large {
        Large(int val) {
            for (int i = 0; i < 1000; ++i) {
                data[i] = val;
            }
        }
        int data[1000];
    };

    State lua(State::LibBase);
    Metatable<Large>::registerMetatable(lua);
    lua.bindConstructor<Large, int>("Large");

    const char* src = "big = Large(42)";
    lua.loadAndExecuteScript(src);

    Large* big = readVar<Large*>(lua,"big");
    ASSERT_NE(big, nullptr);
    EXPECT_EQ(big->data[0], 42);
    EXPECT_EQ(big->data[999], 42);
}

// ============================================================================
// Method Binding Tests
// ============================================================================

TEST(BindMethodTest, PrimitiveReturn) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::length>("length");

    const char* src = "v = Vec(3, 4); result = v:length()";
    lua.loadAndExecuteScript(src);

    float result = static_cast<float>(readVar<double>(lua,"result"));
    EXPECT_FLOAT_EQ(result, 5.0f);
}

TEST(BindMethodTest, UserdataReturn) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::scaled>("scaled");

    const char* src = "v = Vec(2, 3); result = v:scaled(2.5)";
    lua.loadAndExecuteScript(src);

    Vec* result = readVar<Vec*>(lua,"result");
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->x, 5.0f);
    EXPECT_FLOAT_EQ(result->y, 7.5f);
}

TEST(BindMethodTest, UserdataArg) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::dot>("dot");

    const char* src = "v1 = Vec(3, 4); v2 = Vec(1, 2); result = v1:dot(v2)";
    lua.loadAndExecuteScript(src);

    float result = static_cast<float>(readVar<double>(lua,"result"));
    EXPECT_FLOAT_EQ(result, 11.0f);
}

TEST(BindMethodTest, VoidReturn) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::reset>("reset");

    const char* src = "v = Vec(7, 8); v:reset()";
    lua.loadAndExecuteScript(src);

    Vec* v = readVar<Vec*>(lua,"v");
    ASSERT_NE(v, nullptr);
    EXPECT_FLOAT_EQ(v->x, 0.0f);
    EXPECT_FLOAT_EQ(v->y, 0.0f);
}

TEST(BindMethodTest, MultipleArgs) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::count>("count");

    const char* src = "v = Vec(0, 0); result = v:count(1, 2, 3)";
    lua.loadAndExecuteScript(src);

    int result = readVar<int>(lua,"result");
    EXPECT_EQ(result, 6);
}

TEST(BindMethodTest, WrongSelfType_Errors) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<Other>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindConstructor<Other, int>("Other");
    lua.bindMethod<Vec, &Vec::length>("length");

    const char* src = "o = Other(42); result = Vec.length(o)";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
}

TEST(BindMethodTest, WrongArgType_Errors) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::dot>("dot");

    const char* src = "v = Vec(1, 2); result = v:dot(42)";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
}

TEST(BindMethodTest, MethodAndOperatorsCoexist) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::length>("length");

    const char* src = "v1 = Vec(3, 0); v2 = Vec(0, 4); result = (v1 + v2):length()";
    lua.loadAndExecuteScript(src);

    float result = static_cast<float>(readVar<double>(lua,"result"));
    EXPECT_FLOAT_EQ(result, 5.0f);
}

TEST(BindMethodTest, MultipleMethods) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::length>("length");
    lua.bindMethod<Vec, &Vec::scaled>("scaled");
    lua.bindMethod<Vec, &Vec::dot>("dot");

    const char* src =
        "v1 = Vec(3, 4);"
        "v2 = Vec(1, 0);"
        "len = v1:length();"
        "sc = v1:scaled(0.5);"
        "d = v1:dot(v2)";
    lua.loadAndExecuteScript(src);

    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"len")), 5.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"d")), 3.0f);

    Vec* sc = readVar<Vec*>(lua,"sc");
    ASSERT_NE(sc, nullptr);
    EXPECT_FLOAT_EQ(sc->x, 1.5f);
    EXPECT_FLOAT_EQ(sc->y, 2.0f);
}

TEST(BindMethodTest, MethodOnDifferentTypes) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<Other>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindConstructor<Other, int>("Other");
    lua.bindMethod<Vec, &Vec::length>("length");
    lua.bindMethod<Other, &Other::doubled>("doubled");

    const char* src =
        "v = Vec(3, 4);"
        "o = Other(21);"
        "vLen = v:length();"
        "oDbl = o:doubled()";
    lua.loadAndExecuteScript(src);

    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"vLen")), 5.0f);
    EXPECT_EQ(readVar<int>(lua,"oDbl"), 42);
}

// ============================================================================
// __tostring Tests
// ============================================================================

TEST(BindToStringTest, AutoRegisteredForTypesWithToString) {
    State lua(State::LibBase);
    Metatable<Stringable>::registerMetatable(lua);
    lua.bindConstructor<Stringable, int>("Stringable");

    const char* src = "s = Stringable(42); result = tostring(s)";
    lua.loadAndExecuteScript(src);

    std::string result = readVar<std::string>(lua,"result");
    EXPECT_EQ(result, "Stringable(42)");
}

TEST(BindToStringTest, NotRegisteredForTypesWithoutToString) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");

    const char* src = "v = Vec(1, 2); result = tostring(v)";
    lua.loadAndExecuteScript(src);

    // Without toString(), Lua falls back to the default "<__name>: <address>" format
    // (luaL_newmetatable auto-sets __name to the registered metatable name).
    std::string result = readVar<std::string>(lua,"result");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find(": "), std::string::npos);
}

TEST(BindToStringTest, UsedByLuaConcatenation) {
    State lua(State::LibBase);
    Metatable<Stringable>::registerMetatable(lua);
    lua.bindConstructor<Stringable, int>("Stringable");

    const char* src = "s = Stringable(7); result = '' .. tostring(s)";
    lua.loadAndExecuteScript(src);

    std::string result = readVar<std::string>(lua,"result");
    EXPECT_EQ(result, "Stringable(7)");
}

// ============================================================================
// Comparison Operator Auto-Registration Tests (__lt, __le)
// ============================================================================

TEST(BindComparisonTest, LessThan) {
    State lua(State::LibBase);
    Metatable<Comparable>::registerMetatable(lua);
    lua.bindConstructor<Comparable, int>("Cmp");

    const char* src = R"(
        a = Cmp(3); b = Cmp(5)
        ltTrue  = a < b
        ltFalse = b < a
        ltSelf  = a < a
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"ltTrue"));
    EXPECT_FALSE(readVar<bool>(lua,"ltFalse"));
    EXPECT_FALSE(readVar<bool>(lua,"ltSelf"));
}

TEST(BindComparisonTest, LessEqual) {
    State lua(State::LibBase);
    Metatable<Comparable>::registerMetatable(lua);
    lua.bindConstructor<Comparable, int>("Cmp");

    const char* src = R"(
        a = Cmp(3); b = Cmp(5); c = Cmp(3)
        leLess  = a <= b
        leEqual = a <= c
        leMore  = b <= a
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"leLess"));
    EXPECT_TRUE(readVar<bool>(lua,"leEqual"));
    EXPECT_FALSE(readVar<bool>(lua,"leMore"));
}

// ============================================================================
// Mixed-Type Operator Tests
// ============================================================================

namespace {

struct ScalarVec {
    float x, y;
    explicit ScalarVec(float ax = 0, float ay = 0) : x(ax), y(ay) {}
    ScalarVec operator+(const ScalarVec& rhs) const { return ScalarVec(x + rhs.x, y + rhs.y); }
    ScalarVec operator-(const ScalarVec& rhs) const { return ScalarVec(x - rhs.x, y - rhs.y); }
    ScalarVec operator*(double s) const { return ScalarVec(static_cast<float>(x * s), static_cast<float>(y * s)); }
    ScalarVec operator/(double s) const { return ScalarVec(static_cast<float>(x / s), static_cast<float>(y / s)); }
};

inline ScalarVec operator*(double s, const ScalarVec& v) {
    return ScalarVec(static_cast<float>(s * v.x), static_cast<float>(s * v.y));
}

} // namespace

TEST(BindMixedOpTest, VecMulScalar) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");

    const char* src = "v = Vec(2, 3); result = v * 2.5";
    lua.loadAndExecuteScript(src);
    ScalarVec* r = readVar<ScalarVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->x, 5.0f);
    EXPECT_FLOAT_EQ(r->y, 7.5f);
}

TEST(BindMixedOpTest, ScalarMulVec) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");

    const char* src = "v = Vec(2, 3); result = 2.5 * v";
    lua.loadAndExecuteScript(src);
    ScalarVec* r = readVar<ScalarVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->x, 5.0f);
    EXPECT_FLOAT_EQ(r->y, 7.5f);
}

TEST(BindMixedOpTest, VecDivScalar) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");

    const char* src = "v = Vec(10, 20); result = v / 4";
    lua.loadAndExecuteScript(src);
    ScalarVec* r = readVar<ScalarVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->x, 2.5f);
    EXPECT_FLOAT_EQ(r->y, 5.0f);
}

TEST(BindMixedOpTest, SameTypeStillWorks) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");

    const char* src = "a = Vec(1, 2); b = Vec(3, 4); result = a + b";
    lua.loadAndExecuteScript(src);
    ScalarVec* r = readVar<ScalarVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->x, 4.0f);
    EXPECT_FLOAT_EQ(r->y, 6.0f);
}

TEST(BindMixedOpTest, UnsupportedScalarErrors) {
    // ScalarVec has no operator+(double) — only T+T. Mixed should error.
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");

    const char* src = "v = Vec(1, 2); result = v + 5";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);  // Vec has no operator+(double)
}

// A type with the full set of mixed-type arithmetic operators, used to
// exercise the AddOp T+double / double+T and DivOp double/T paths that
// ScalarVec does not cover.
namespace {

struct ArithVec {
    float v;
    explicit ArithVec(float val = 0) : v(val) {}
    ArithVec operator+(double s) const { return ArithVec(static_cast<float>(v + s)); }
    ArithVec operator/(double s) const { return ArithVec(static_cast<float>(v / s)); }
};

inline ArithVec operator+(double s, const ArithVec& a) {
    return ArithVec(static_cast<float>(s + a.v));
}
inline ArithVec operator/(double s, const ArithVec& a) {
    return ArithVec(static_cast<float>(s / a.v));
}

} // namespace

TEST(BindMixedOpTest, VecPlusScalar) {
    State lua(State::LibBase);
    Metatable<ArithVec>::registerMetatable(lua);
    lua.bindConstructor<ArithVec, float>("Vec");

    const char* src = "v = Vec(10); result = v + 5";
    lua.loadAndExecuteScript(src);
    ArithVec* r = readVar<ArithVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->v, 15.0f);
}

TEST(BindMixedOpTest, ScalarPlusVec) {
    State lua(State::LibBase);
    Metatable<ArithVec>::registerMetatable(lua);
    lua.bindConstructor<ArithVec, float>("Vec");

    const char* src = "v = Vec(10); result = 5 + v";
    lua.loadAndExecuteScript(src);
    ArithVec* r = readVar<ArithVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->v, 15.0f);
}

TEST(BindMixedOpTest, ScalarDivVec) {
    State lua(State::LibBase);
    Metatable<ArithVec>::registerMetatable(lua);
    lua.bindConstructor<ArithVec, float>("Vec");

    const char* src = "v = Vec(2); result = 10 / v";
    lua.loadAndExecuteScript(src);
    ArithVec* r = readVar<ArithVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->v, 5.0f);
}

TEST(BindMixedOpTest, UnaryMinusNotRegisteredForTypesWithoutIt) {
    // Vec has no operator-() (unary). Lua's __unm should be omitted,
    // and `-v` should error rather than silently succeed.
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");

    const char* src = "v = Vec(1, 2); result = -v";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
}

// ============================================================================
// Static Field / Static Function Tests
// ============================================================================

namespace {
ScalarVec makeUnitX() { return ScalarVec(1, 0); }
ScalarVec makeFromAngle(double radians) {
    return ScalarVec(static_cast<float>(std::cos(radians)),
                     static_cast<float>(std::sin(radians)));
}
int addThree(int a, int b, int c) { return a + b + c; }
}

TEST(BindStaticTest, StaticNumberField) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");
    Bind::staticField(lua, "Vec", "EPSILON", 0.001);

    const char* src = "result = Vec.EPSILON";
    lua.loadAndExecuteScript(src);
    EXPECT_DOUBLE_EQ(readVar<double>(lua,"result"), 0.001);
}

TEST(BindStaticTest, StaticStringField) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");
    Bind::staticField(lua, "Vec", "TYPENAME", "ScalarVec");

    const char* src = "result = Vec.TYPENAME";
    lua.loadAndExecuteScript(src);
    EXPECT_EQ(readVar<std::string>(lua,"result"), "ScalarVec");
}

TEST(BindStaticTest, StaticFunctionNoArgs) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");
    Bind::staticFunction<&makeUnitX>(lua, "Vec", "unitX");

    const char* src = "result = Vec.unitX()";
    lua.loadAndExecuteScript(src);
    ScalarVec* r = readVar<ScalarVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->x, 1.0f);
    EXPECT_FLOAT_EQ(r->y, 0.0f);
}

TEST(BindStaticTest, StaticFunctionWithArgs) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");
    Bind::staticFunction<&makeFromAngle>(lua, "Vec", "fromAngle");

    const char* src = "result = Vec.fromAngle(0)";
    lua.loadAndExecuteScript(src);
    ScalarVec* r = readVar<ScalarVec*>(lua,"result");
    ASSERT_NE(r, nullptr);
    EXPECT_FLOAT_EQ(r->x, 1.0f);
    EXPECT_FLOAT_EQ(r->y, 0.0f);
}

TEST(BindStaticTest, StaticFunctionPrimitiveReturn) {
    State lua(State::LibBase);
    Metatable<ScalarVec>::registerMetatable(lua);
    lua.bindConstructor<ScalarVec, float, float>("Vec");
    Bind::staticFunction<&addThree>(lua, "Vec", "sum");

    const char* src = "result = Vec.sum(1, 2, 3)";
    lua.loadAndExecuteScript(src);
    EXPECT_EQ(readVar<int>(lua,"result"), 6);
}

TEST(BindComparisonTest, GreaterDerivesFromLessThan) {
    State lua(State::LibBase);
    Metatable<Comparable>::registerMetatable(lua);
    lua.bindConstructor<Comparable, int>("Cmp");

    // Lua maps a > b to b < a, so __lt is sufficient for >
    const char* src = "a = Cmp(7); b = Cmp(3); result = a > b";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"result"));
}

TEST(BindComparisonTest, NotRegisteredForTypesWithoutComparison) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");

    // Vec has no operator< / operator<= -- Lua should error on comparison
    const char* src = "a = Vec(1, 2); b = Vec(3, 4); result = a < b";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
}

TEST(BindComparisonTest, EqualNotRegisteredForTypesWithoutOp) {
    // Vec has no operator==. Without __eq, Lua falls back to raw identity
    // comparison: two distinct userdata objects compare unequal even when
    // their contents match.
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");

    const char* src =
        "a = Vec(1, 2); b = Vec(1, 2);"
        " sameRef = (a == a); diffRef = (a == b)";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"sameRef"));   // identity → equal
    EXPECT_FALSE(readVar<bool>(lua,"diffRef"));  // distinct objects → unequal
}

// ============================================================================
// Property Binding Tests
// ============================================================================

TEST(BindPropertyTest, ReadPrimitiveField) {
    State lua(State::LibBase);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::x>("x");
    lua.bindProperty<Particle, &Particle::y>("y");
    lua.bindProperty<Particle, &Particle::health>("health");

    const char* src = R"(
        p = Particle(3, 4, 100)
        rx = p.x; ry = p.y; rh = p.health
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"rx")), 3.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"ry")), 4.0f);
    EXPECT_EQ(readVar<int>(lua,"rh"), 100);
}

TEST(BindPropertyTest, WritePrimitiveField) {
    State lua(State::LibBase);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::x>("x");
    lua.bindProperty<Particle, &Particle::health>("health");

    const char* src = R"(
        p = Particle(1, 1, 50)
        p.x = 99.5
        p.health = 25
    )";
    lua.loadAndExecuteScript(src);

    Particle* p = readVar<Particle*>(lua,"p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 99.5f);
    EXPECT_EQ(p->health, 25);
}

TEST(BindPropertyTest, ReadStringField) {
    State lua(State::LibBase);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::name>("name");

    const char* src = "p = Particle(0, 0, 1); n = p.name";
    lua.loadAndExecuteScript(src);
    EXPECT_EQ(readVar<std::string>(lua,"n"), "particle");
}

TEST(BindPropertyTest, ReadUserdataField) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::velocity>("velocity");

    const char* src = "p = Particle(0, 0, 1); v = p.velocity";
    lua.loadAndExecuteScript(src);

    Vec* v = readVar<Vec*>(lua,"v");
    ASSERT_NE(v, nullptr);
    EXPECT_FLOAT_EQ(v->x, 0.0f);
    EXPECT_FLOAT_EQ(v->y, 0.0f);
}

TEST(BindPropertyTest, WriteUserdataField) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::velocity>("velocity");

    const char* src = "p = Particle(0, 0, 1); p.velocity = Vec(7, 8)";
    lua.loadAndExecuteScript(src);

    Particle* p = readVar<Particle*>(lua,"p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->velocity.x, 7.0f);
    EXPECT_FLOAT_EQ(p->velocity.y, 8.0f);
}

TEST(BindPropertyTest, UnknownPropertyRead_ReturnsNil) {
    State lua(State::LibBase);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::x>("x");

    const char* src = "p = Particle(1, 2, 3); result = (p.nonExistent == nil)";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"result"));
}

TEST(BindPropertyTest, UnknownPropertyWrite_Errors) {
    State lua(State::LibBase);
    Metatable<Particle>::registerMetatable(lua);
    lua.bindConstructor<Particle, float, float, int>("Particle");
    lua.bindProperty<Particle, &Particle::x>("x");

    const char* src = "p = Particle(1, 2, 3); p.nonExistent = 5";
    lua.installErrorHandler<ThrowDecorator>();
    EXPECT_THROW(lua.loadAndExecuteScript(src), LuaException);
}

TEST(BindPropertyTest, PropertyAndMethodCoexist) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindProperty<Vec, &Vec::x>("x");
    lua.bindProperty<Vec, &Vec::y>("y");
    lua.bindMethod<Vec, &Vec::length>("length");

    const char* src = R"(
        v = Vec(3, 4)
        readX = v.x
        readY = v.y
        len = v:length()
        v.x = 6
        afterX = v.x
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"readX")), 3.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"readY")), 4.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"len")), 5.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"afterX")), 6.0f);
}

// ============================================================================
// Prerequisite-violation tests: misuse must throw, not silently no-op.
// ============================================================================

TEST(BindPrerequisiteTest, MethodWithoutMetatable_Throws) {
    State lua(State::LibBase);
    // Intentionally skip Metatable<Point>::registerMetatable.
    EXPECT_THROW(
        (lua.bindMethod<Point, &Point::operator+>("plus")),
        std::runtime_error);
}

TEST(BindPrerequisiteTest, PropertyWithoutMetatable_Throws) {
    State lua(State::LibBase);
    // Intentionally skip Metatable<Point>::registerMetatable.
    EXPECT_THROW(
        (lua.bindProperty<Point, &Point::x>("x")),
        std::runtime_error);
}

TEST(BindPrerequisiteTest, StaticFieldWithoutConstructor_Throws) {
    State lua(State::LibBase);
    // No bindConstructor → "Point" is not a global table.
    EXPECT_THROW(
        lua.bindStaticField("Point", "EPSILON", 0.001f),
        std::runtime_error);
}

static int prereqAnswerFn() { return 42; }

TEST(BindPrerequisiteTest, StaticFunctionWithoutConstructor_Throws) {
    State lua(State::LibBase);
    EXPECT_THROW(
        lua.bindStaticFunction<&prereqAnswerFn>("Point", "answer"),
        std::runtime_error);
}

// ============================================================================
// Return-Type Handling: T*, T&, null pointers
// ============================================================================
// Methods/functions that return T* or T& must yield a Lua userdata with the
// proper metatable (not a raw lightuserdata), and must not destructively move
// from aliased C++ objects.

namespace {

struct MovableTracker {
    int value;
    bool wasMovedFrom;
    explicit MovableTracker(int v) : value(v), wasMovedFrom(false) {}
    MovableTracker(const MovableTracker& o)
        : value(o.value), wasMovedFrom(false) {}
    MovableTracker(MovableTracker&& o) noexcept
        : value(o.value), wasMovedFrom(false) {
        o.wasMovedFrom = true;
        o.value = -999;
    }
    MovableTracker& operator=(const MovableTracker&) = default;
    int getValue() const { return value; }
};

struct ReturnSource {
    Vec stored;
    MovableTracker tracker;
    ReturnSource() : stored(1.5f, 2.5f), tracker(42) {}

    Vec* getVecPtr() { return &stored; }
    Vec* getNullVec() { return nullptr; }
    Vec& getVecRef() { return stored; }
    const Vec& getVecConstRef() const { return stored; }

    MovableTracker& getTrackerRef() { return tracker; }
    bool trackerWasMoved() const { return tracker.wasMovedFrom; }
    int trackerValue() const { return tracker.value; }

    float storedX() const { return stored.x; }
};

} // namespace

TEST(BindReturnTest, PointerReturn_WrapsAsUserdataWithMetatable) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    lua.bindMethod<ReturnSource, &ReturnSource::getVecPtr>("getVecPtr");
    lua.bindMethod<Vec, &Vec::length>("length");

    // If getVecPtr returned raw lightuserdata, v:length() would fail because
    // lightuserdata has no metatable. Wrapping as userdata makes it work.
    const char* src = "s = Source(); v = s:getVecPtr(); len = v:length()";
    lua.loadAndExecuteScript(src);
    double len = readVar<double>(lua,"len");
    EXPECT_NEAR(len, std::sqrt(1.5 * 1.5 + 2.5 * 2.5), 1e-5);
}

TEST(BindReturnTest, NullPointerReturn_BecomesNil) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    lua.bindMethod<ReturnSource, &ReturnSource::getNullVec>("getNullVec");

    const char* src = "s = Source(); v = s:getNullVec(); isNil = (v == nil)";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"isNil"));
}

TEST(BindReturnTest, PointerReturn_IsCopy_MutationDoesNotPropagate) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    lua.bindMethod<ReturnSource, &ReturnSource::getVecPtr>("getVecPtr");
    lua.bindMethod<ReturnSource, &ReturnSource::storedX>("storedX");
    lua.bindProperty<Vec, &Vec::x>("x");

    // Copy semantics: mutating the Lua-side userdata does NOT change the C++
    // object the pointer originally referred to. Documented as intentional.
    const char* src = R"(
        s = Source()
        v = s:getVecPtr()
        v.x = 99
        copyX = v.x
        origX = s:storedX()
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"copyX")), 99.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(readVar<double>(lua,"origX")), 1.5f);
}

TEST(BindReturnTest, ReferenceReturn_DoesNotMoveFromAliased) {
    State lua(State::LibBase);
    Metatable<MovableTracker>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    lua.bindMethod<ReturnSource, &ReturnSource::getTrackerRef>("getTrackerRef");
    lua.bindMethod<ReturnSource, &ReturnSource::trackerWasMoved>("wasMoved");
    lua.bindMethod<ReturnSource, &ReturnSource::trackerValue>("trackerValue");

    // Previously the dispatcher did std::move(result) where result was a
    // reference, leaving the source's tracker in moved-from state. After the
    // fix, references copy rather than move.
    const char* src = R"(
        s = Source()
        t = s:getTrackerRef()
        movedAfter = s:wasMoved()
        valueAfter = s:trackerValue()
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_FALSE(readVar<bool>(lua,"movedAfter"));
    EXPECT_EQ(readVar<int>(lua,"valueAfter"), 42);
}

TEST(BindReturnTest, ConstReferenceReturn_AlsoCopies) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    lua.bindMethod<ReturnSource, &ReturnSource::getVecConstRef>("getVecConstRef");
    lua.bindMethod<Vec, &Vec::length>("length");

    // Two successive const-ref returns should both yield valid Vecs with the
    // same content — proving the source's Vec wasn't destroyed by the first
    // call (which the old std::move-from-reference code would have done).
    const char* src = R"(
        s = Source()
        v1 = s:getVecConstRef()
        v2 = s:getVecConstRef()
        len1 = v1:length()
        len2 = v2:length()
    )";
    lua.loadAndExecuteScript(src);
    EXPECT_DOUBLE_EQ(readVar<double>(lua,"len1"),
                     readVar<double>(lua,"len2"));
    EXPECT_NEAR(readVar<double>(lua,"len1"),
                std::sqrt(1.5 * 1.5 + 2.5 * 2.5), 1e-5);
}

namespace {
static Vec* freeFuncReturnsVecPtr() {
    static Vec s(7.0f, 24.0f);
    return &s;
}
static Vec* freeFuncReturnsNullVec() { return nullptr; }
}

TEST(BindReturnTest, FreeFunction_PointerReturn_WrapsAsUserdata) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    Bind::staticFunction<&freeFuncReturnsVecPtr>(lua, "Source", "globalVec");
    lua.bindMethod<Vec, &Vec::length>("length");

    const char* src = "v = Source.globalVec(); len = v:length()";
    lua.loadAndExecuteScript(src);
    EXPECT_NEAR(readVar<double>(lua,"len"), 25.0, 1e-5);
}

TEST(BindReturnTest, FreeFunction_NullPointerReturn_BecomesNil) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    Metatable<ReturnSource>::registerMetatable(lua);
    lua.bindConstructor<ReturnSource>("Source");
    Bind::staticFunction<&freeFuncReturnsNullVec>(lua, "Source", "noVec");

    const char* src = "v = Source.noVec(); isNil = (v == nil)";
    lua.loadAndExecuteScript(src);
    EXPECT_TRUE(readVar<bool>(lua,"isNil"));
}

} // namespace Lua
