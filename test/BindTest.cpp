#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Metatable.hpp>

#include <cmath>
#include <string>

namespace Lua {

// ============================================================================
// Test Types
// ============================================================================

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

// ============================================================================
// Constructor Binding Tests
// ============================================================================

TEST(BindConstructorTest, BasicConstructor) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "p = Point(3.5, 4.5)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* p = lua.readVariable<Point*>("p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 3.5f);
    EXPECT_FLOAT_EQ(p->y, 4.5f);
}

TEST(BindConstructorTest, SingleArgumentConstructor) {
    State lua(State::LibBase);
    Metatable<Counter>::registerMetatable(lua);
    lua.bindConstructor<Counter, int>("Counter");

    const char* src = "c = Counter(42)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Counter* c = lua.readVariable<Counter*>("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->value, 42);
}

TEST(BindConstructorTest, ConstructorWithOperators) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "p1 = Point(1, 2); p2 = Point(3, 4); result = p1 + p2";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* result = lua.readVariable<Point*>("result");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* p = lua.readVariable<Point*>("p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 1.5f);
    EXPECT_FLOAT_EQ(p->y, 2.5f);

    Counter* c = lua.readVariable<Counter*>("c");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Line* line = lua.readVariable<Line*>("line");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Line* line = lua.readVariable<Line*>("line");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    int id = lua.readVariable<int>("id");
    EXPECT_EQ(id, 123);

    Resource* r = lua.readVariable<Resource*>("r");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Resource* r1 = lua.readVariable<Resource*>("r1");
    Resource* r2 = lua.readVariable<Resource*>("r2");
    Resource* r3 = lua.readVariable<Resource*>("r3");

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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* p1 = lua.readVariable<Point*>("p1");
    ASSERT_NE(p1, nullptr);
    EXPECT_FLOAT_EQ(p1->x, 1.0f);
    EXPECT_FLOAT_EQ(p1->y, 2.0f);

    Point* p3 = lua.readVariable<Point*>("p3");
    ASSERT_NE(p3, nullptr);
    EXPECT_FLOAT_EQ(p3->x, 5.0f);
    EXPECT_FLOAT_EQ(p3->y, 6.0f);
}

TEST(BindConstructorTest, ComplexExpression) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src = "result = Point(1, 2) + Point(3, 4) + Point(5, 6)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* result = lua.readVariable<Point*>("result");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    lua.loadAndExecuteScript("first = points[1]");
    Point* first = lua.readVariable<Point*>("first");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Counter* sum = lua.readVariable<Counter*>("sum");
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->value, 15);
}

TEST(BindConstructorTest, ErrorHandling_WrongArgCount) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.bindConstructor<Point, float, float>("Point");

    const char* src1 = "p = Point(1)";
    int status1 = lua.loadAndExecuteScript(src1);
    EXPECT_EQ(status1, 0);

    Point* p1 = lua.readVariable<Point*>("p");
    ASSERT_NE(p1, nullptr);
    EXPECT_FLOAT_EQ(p1->x, 1.0f);
    EXPECT_FLOAT_EQ(p1->y, 0.0f);

    const char* src2 = "p = Point(1, 2, 3, 4)";
    int status2 = lua.loadAndExecuteScript(src2);
    EXPECT_EQ(status2, 0);

    Point* p2 = lua.readVariable<Point*>("p");
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
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);
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
    int status = lua.loadAndExecuteScript(src);
    EXPECT_EQ(status, 0);

    Empty* e = lua.readVariable<Empty*>("e");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Large* big = lua.readVariable<Large*>("big");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    float result = static_cast<float>(lua.readVariable<double>("result"));
    EXPECT_FLOAT_EQ(result, 5.0f);
}

TEST(BindMethodTest, UserdataReturn) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::scaled>("scaled");

    const char* src = "v = Vec(2, 3); result = v:scaled(2.5)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Vec* result = lua.readVariable<Vec*>("result");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    float result = static_cast<float>(lua.readVariable<double>("result"));
    EXPECT_FLOAT_EQ(result, 11.0f);
}

TEST(BindMethodTest, VoidReturn) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::reset>("reset");

    const char* src = "v = Vec(7, 8); v:reset()";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Vec* v = lua.readVariable<Vec*>("v");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    int result = lua.readVariable<int>("result");
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
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);
}

TEST(BindMethodTest, WrongArgType_Errors) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::dot>("dot");

    const char* src = "v = Vec(1, 2); result = v:dot(42)";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0);
}

TEST(BindMethodTest, MethodAndOperatorsCoexist) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");
    lua.bindMethod<Vec, &Vec::length>("length");

    const char* src = "v1 = Vec(3, 0); v2 = Vec(0, 4); result = (v1 + v2):length()";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    float result = static_cast<float>(lua.readVariable<double>("result"));
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    EXPECT_FLOAT_EQ(static_cast<float>(lua.readVariable<double>("len")), 5.0f);
    EXPECT_FLOAT_EQ(static_cast<float>(lua.readVariable<double>("d")), 3.0f);

    Vec* sc = lua.readVariable<Vec*>("sc");
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
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    EXPECT_FLOAT_EQ(static_cast<float>(lua.readVariable<double>("vLen")), 5.0f);
    EXPECT_EQ(lua.readVariable<int>("oDbl"), 42);
}

// ============================================================================
// __tostring Tests
// ============================================================================

TEST(BindToStringTest, AutoRegisteredForTypesWithToString) {
    State lua(State::LibBase);
    Metatable<Stringable>::registerMetatable(lua);
    lua.bindConstructor<Stringable, int>("Stringable");

    const char* src = "s = Stringable(42); result = tostring(s)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    std::string result = lua.readVariable<std::string>("result");
    EXPECT_EQ(result, "Stringable(42)");
}

TEST(BindToStringTest, NotRegisteredForTypesWithoutToString) {
    State lua(State::LibBase);
    Metatable<Vec>::registerMetatable(lua);
    lua.bindConstructor<Vec, float, float>("Vec");

    const char* src = "v = Vec(1, 2); result = tostring(v)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    // Without toString(), Lua falls back to the default "<__name>: <address>" format
    // (luaL_newmetatable auto-sets __name to the registered metatable name).
    std::string result = lua.readVariable<std::string>("result");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find(": "), std::string::npos);
}

TEST(BindToStringTest, UsedByLuaConcatenation) {
    State lua(State::LibBase);
    Metatable<Stringable>::registerMetatable(lua);
    lua.bindConstructor<Stringable, int>("Stringable");

    const char* src = "s = Stringable(7); result = '' .. tostring(s)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    std::string result = lua.readVariable<std::string>("result");
    EXPECT_EQ(result, "Stringable(7)");
}

} // namespace Lua
