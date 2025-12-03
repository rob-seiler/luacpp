#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Metatable.hpp>

namespace Lua {

// ============================================================================
// Test Types
// ============================================================================

// Simple type for basic constructor tests
struct Point {
    Point(float x_, float y_) : x(x_), y(y_) {}
    Point operator+(const Point& rhs) const { return Point(x + rhs.x, y + rhs.y); }
    float x, y;
};

// Non-copyable type to verify direct construction (no heap allocation + copy)
struct Resource {
    explicit Resource(int id_) : id(id_), moveCount(0) {
        // Track construction
    }

    // Delete copy constructor and assignment
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;

    // Allow move operations
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
    int moveCount; // Track how many times it was moved
};

// Type with multiple constructor arguments including userdata
struct Line {
    Line(Point start_, Point end_) : start(start_), end(end_) {}
    Point start;
    Point end;
};

// Type with single int argument
struct Counter {
    explicit Counter(int value_) : value(value_) {}
    Counter operator+(const Counter& rhs) const { return Counter(value + rhs.value); }
    int value;
};

// ============================================================================
// Basic Constructor Tests
// ============================================================================

TEST(ConstructorRegistryTest, BasicConstructor) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    const char* src = "p = Point(3.5, 4.5)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* p = lua.readVariable<Point*>("p");
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->x, 3.5f);
    EXPECT_FLOAT_EQ(p->y, 4.5f);
}

TEST(ConstructorRegistryTest, SingleArgumentConstructor) {
    State lua(State::LibBase);
    Metatable<Counter>::registerMetatable(lua);
    lua.registerConstructor<Counter, int>("Counter");

    const char* src = "c = Counter(42)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Counter* c = lua.readVariable<Counter*>("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->value, 42);
}

TEST(ConstructorRegistryTest, ConstructorWithOperators) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    const char* src = "p1 = Point(1, 2); p2 = Point(3, 4); result = p1 + p2";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* result = lua.readVariable<Point*>("result");
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->x, 4.0f);
    EXPECT_FLOAT_EQ(result->y, 6.0f);
}

TEST(ConstructorRegistryTest, MultipleConstructors) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Counter>::registerMetatable(lua);

    lua.registerConstructor<Point, float, float>("Point");
    lua.registerConstructor<Counter, int>("Counter");

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

// ============================================================================
// Advanced Constructor Tests
// ============================================================================

TEST(ConstructorRegistryTest, ConstructorWithUserdataArgs) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Line>::registerMetatable(lua);

    lua.registerConstructor<Point, float, float>("Point");
    lua.registerConstructor<Line, Point, Point>("Line");

    const char* src = "start = Point(0, 0); finish = Point(10, 20); line = Line(start, finish)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Line* line = lua.readVariable<Line*>("line");
    ASSERT_NE(line, nullptr);
    EXPECT_FLOAT_EQ(line->start.x, 0.0f);
    EXPECT_FLOAT_EQ(line->start.y, 0.0f);
    EXPECT_FLOAT_EQ(line->end.x, 10.0f);
    EXPECT_FLOAT_EQ(line->end.y, 20.0f);
}

TEST(ConstructorRegistryTest, ConstructorWithInlineUserdataArgs) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Line>::registerMetatable(lua);

    lua.registerConstructor<Point, float, float>("Point");
    lua.registerConstructor<Line, Point, Point>("Line");

    // Pass constructed points directly without storing in variables
    const char* src = "line = Line(Point(1, 2), Point(3, 4))";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Line* line = lua.readVariable<Line*>("line");
    ASSERT_NE(line, nullptr);
    EXPECT_FLOAT_EQ(line->start.x, 1.0f);
    EXPECT_FLOAT_EQ(line->start.y, 2.0f);
    EXPECT_FLOAT_EQ(line->end.x, 3.0f);
    EXPECT_FLOAT_EQ(line->end.y, 4.0f);
}

// ============================================================================
// Non-Copyable Type Tests (verifies fix for Issue #1)
// ============================================================================

TEST(ConstructorRegistryTest, NonCopyableType) {
    State lua(State::LibBase);
    Metatable<Resource>::registerMetatable(lua);
    lua.registerConstructor<Resource, int>("Resource");

    // Register getter to verify the resource ID
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

    // Verify the resource was constructed directly (not copied)
    Resource* r = lua.readVariable<Resource*>("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->id, 123);
    // If our fix is correct, moveCount should be 0 (constructed in place)
    // If the old code ran, this would fail to compile or have moves
    EXPECT_EQ(r->moveCount, 0);
}

TEST(ConstructorRegistryTest, MultipleNonCopyableInstances) {
    State lua(State::LibBase);
    Metatable<Resource>::registerMetatable(lua);
    lua.registerConstructor<Resource, int>("Resource");

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

    // All should be constructed in place (0 moves)
    EXPECT_EQ(r1->moveCount, 0);
    EXPECT_EQ(r2->moveCount, 0);
    EXPECT_EQ(r3->moveCount, 0);
}

// ============================================================================
// Usage Pattern Tests
// ============================================================================

TEST(ConstructorRegistryTest, ConstructorCallSemantics) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    // Verify we can call it multiple ways
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

TEST(ConstructorRegistryTest, ComplexExpression) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    // Use constructor in complex expression
    const char* src = "result = Point(1, 2) + Point(3, 4) + Point(5, 6)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Point* result = lua.readVariable<Point*>("result");
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->x, 9.0f);
    EXPECT_FLOAT_EQ(result->y, 12.0f);
}

TEST(ConstructorRegistryTest, ConstructorInTable) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    // Store constructed objects in table
    const char* src = R"(
        points = {
            Point(1, 2),
            Point(3, 4),
            Point(5, 6)
        }
    )";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    // Access table entry
    lua.loadAndExecuteScript("first = points[1]");
    Point* first = lua.readVariable<Point*>("first");
    ASSERT_NE(first, nullptr);
    EXPECT_FLOAT_EQ(first->x, 1.0f);
    EXPECT_FLOAT_EQ(first->y, 2.0f);
}

TEST(ConstructorRegistryTest, ConstructorInLoop) {
    State lua(State::LibBase);
    Metatable<Counter>::registerMetatable(lua);
    lua.registerConstructor<Counter, int>("Counter");

    const char* src = R"(
        sum = Counter(0)
        for i = 1, 5 do
            sum = sum + Counter(i)
        end
    )";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Counter* sum = lua.readVariable<Counter*>("sum");
    ASSERT_NE(sum, nullptr);
    EXPECT_EQ(sum->value, 15); // 0 + 1 + 2 + 3 + 4 + 5
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(ConstructorRegistryTest, ErrorHandling_WrongArgCount) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    // Too few arguments - Lua will pass nil/0 for missing args
    // This doesn't error, just results in default values (0)
    const char* src1 = "p = Point(1)";
    int status1 = lua.loadAndExecuteScript(src1);
    EXPECT_EQ(status1, 0); // Succeeds with nil converted to 0

    Point* p1 = lua.readVariable<Point*>("p");
    ASSERT_NE(p1, nullptr);
    EXPECT_FLOAT_EQ(p1->x, 1.0f);
    EXPECT_FLOAT_EQ(p1->y, 0.0f); // Missing arg becomes 0

    // Too many arguments is OK in Lua (extras are ignored)
    const char* src2 = "p = Point(1, 2, 3, 4)";
    int status2 = lua.loadAndExecuteScript(src2);
    EXPECT_EQ(status2, 0);

    Point* p2 = lua.readVariable<Point*>("p");
    ASSERT_NE(p2, nullptr);
    EXPECT_FLOAT_EQ(p2->x, 1.0f);
    EXPECT_FLOAT_EQ(p2->y, 2.0f);
}

TEST(ConstructorRegistryTest, ErrorHandling_WrongUserdataType) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    Metatable<Line>::registerMetatable(lua);
    Metatable<Counter>::registerMetatable(lua);

    lua.registerConstructor<Point, float, float>("Point");
    lua.registerConstructor<Line, Point, Point>("Line");
    lua.registerConstructor<Counter, int>("Counter");

    // Try to pass Counter where Point is expected
    const char* src = "c = Counter(5); line = Line(c, c)";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_NE(status, 0); // Should fail with type error
}

TEST(ConstructorRegistryTest, ErrorHandling_NilArgument) {
    State lua(State::LibBase);
    Metatable<Point>::registerMetatable(lua);
    lua.registerConstructor<Point, float, float>("Point");

    // Nil argument should error or result in 0
    const char* src = "p = Point(nil, 2)";
    int status = lua.loadAndExecuteScript(src);
    // Behavior depends on getArgument implementation
    // Just verify it doesn't crash
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(ConstructorRegistryTest, ZeroSizedType) {
    struct Empty {
        Empty() = default;
    };

    State lua(State::LibBase);
    Metatable<Empty>::registerMetatable(lua);
    lua.registerConstructor<Empty>("Empty");

    const char* src = "e = Empty()";
    int status = lua.loadAndExecuteScript(src);
    EXPECT_EQ(status, 0);

    Empty* e = lua.readVariable<Empty*>("e");
    EXPECT_NE(e, nullptr);
}

TEST(ConstructorRegistryTest, LargeType) {
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
    lua.registerConstructor<Large, int>("Large");

    const char* src = "big = Large(42)";
    EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

    Large* big = lua.readVariable<Large*>("big");
    ASSERT_NE(big, nullptr);
    EXPECT_EQ(big->data[0], 42);
    EXPECT_EQ(big->data[999], 42);
}

} // namespace Lua
