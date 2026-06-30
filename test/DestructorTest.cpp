#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Metatable.hpp>
#include <luacpp/Bind.hpp>
#include <lua/lua.hpp>

namespace Lua {

// ============================================================================
// Test Types
// ============================================================================

// Global counters for tracking constructor/destructor calls
static int g_constructorCalls = 0;
static int g_destructorCalls = 0;

// Type with non-trivial destructor
struct ResourceHolder {
    int id;

    ResourceHolder(int id_) : id(id_) {
        g_constructorCalls++;
    }

    ~ResourceHolder() {
        g_destructorCalls++;
    }

    // Non-copyable to ensure clean testing
    ResourceHolder(const ResourceHolder&) = delete;
    ResourceHolder& operator=(const ResourceHolder&) = delete;

    ResourceHolder(ResourceHolder&& other) noexcept : id(other.id) {
        other.id = -1;
        g_constructorCalls++;
    }
};

// Type with trivial destructor (should NOT register __gc)
struct TrivialType {
    int value;
    TrivialType(int v) : value(v) {}
    // Trivially destructible - default destructor
};

// Type with complex resource management
struct FileHandle {
    int fd;
    bool closed;

    FileHandle(int fd_) : fd(fd_), closed(false) {
        g_constructorCalls++;
    }

    ~FileHandle() {
        g_destructorCalls++;
        if (!closed) {
            // Simulate closing a file
            closed = true;
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};

// ============================================================================
// Destructor Registration Tests
// ============================================================================

TEST(DestructorTest, NonTrivialDestructor_IsCalled) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    {
        State lua(State::LibBase);
        Metatable<ResourceHolder>::registerMetatable(lua);
        lua.binding.constructor<ResourceHolder, int>("Resource");

        // Create objects in Lua
        const char* src = R"(
            r1 = Resource(100)
            r2 = Resource(200)
            r3 = Resource(300)
        )";
        lua.loadAndExecuteScript(src);

        // Constructors should have been called
        EXPECT_EQ(g_constructorCalls, 3);
        EXPECT_EQ(g_destructorCalls, 0); // Not destroyed yet

    } // Lua state destroyed here, triggering GC

    // Destructors should now have been called
    EXPECT_EQ(g_destructorCalls, 3);
}

TEST(DestructorTest, ForceGarbageCollection) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    State lua(State::LibBase);
    Metatable<ResourceHolder>::registerMetatable(lua);
    lua.binding.constructor<ResourceHolder, int>("Resource");

    // Create and immediately discard objects
    const char* src = R"(
        for i = 1, 5 do
            local r = Resource(i)
            -- r goes out of scope here
        end
        -- Force garbage collection
        collectgarbage("collect")
    )";
    lua.loadAndExecuteScript(src);

    // All 5 objects should be created
    EXPECT_EQ(g_constructorCalls, 5);

    // After GC, all should be destroyed
    EXPECT_EQ(g_destructorCalls, 5);
}

TEST(DestructorTest, PartialGarbageCollection) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    State lua(State::LibBase);
    Metatable<ResourceHolder>::registerMetatable(lua);
    lua.binding.constructor<ResourceHolder, int>("Resource");

    const char* src = R"(
        -- Create 3 objects, keep 2 referenced globally
        r1 = Resource(1)
        r2 = Resource(2)

        -- Create a local in a do block to ensure it goes out of scope
        do
            local r3 = Resource(3)
            -- r3 goes out of scope at end of do block
        end

        -- Force GC to collect r3
        collectgarbage("collect")
    )";
    lua.loadAndExecuteScript(src);

    EXPECT_EQ(g_constructorCalls, 3);

    // r3 should be destroyed, r1 and r2 still alive
    EXPECT_GE(g_destructorCalls, 1); // At least r3 destroyed (GC timing is non-deterministic)

    // Clear remaining references
    lua.loadAndExecuteScript("r1 = nil; r2 = nil; collectgarbage('collect')");

    // Now all should be destroyed
    EXPECT_EQ(g_destructorCalls, 3);
}

TEST(DestructorTest, ComplexResourceManagement) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    {
        State lua(State::LibBase);
        Metatable<FileHandle>::registerMetatable(lua);
        lua.binding.constructor<FileHandle, int>("FileHandle");

        const char* src = R"(
            -- Simulate opening multiple files
            files = {}
            for i = 1, 10 do
                files[i] = FileHandle(i)
            end

            -- Clear some references
            for i = 1, 5 do
                files[i] = nil
            end

            collectgarbage("collect")
        )";
        lua.loadAndExecuteScript(src);

        EXPECT_EQ(g_constructorCalls, 10);
        EXPECT_EQ(g_destructorCalls, 5); // First 5 should be destroyed

    } // Remaining files destroyed with Lua state

    EXPECT_EQ(g_destructorCalls, 10); // All files closed
}

TEST(DestructorTest, TrivialType_NoGCRegistered) {
    // This test verifies that trivial types don't get __gc registered
    // We can't directly test that __gc isn't registered, but we can verify
    // the type works correctly

    State lua(State::LibBase);
    Metatable<TrivialType>::registerMetatable(lua);
    lua.binding.constructor<TrivialType, int>("Trivial");

    const char* src = R"(
        t = Trivial(42)
        collectgarbage("collect")
    )";
    lua.loadAndExecuteScript(src);

    auto tOpt = lua.variables.read<TrivialType*>("t");
    ASSERT_TRUE(tOpt.has_value());
    EXPECT_EQ((*tOpt)->value, 42);
}

TEST(DestructorTest, NestedScopes) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    State lua(State::LibBase);
    Metatable<ResourceHolder>::registerMetatable(lua);
    lua.binding.constructor<ResourceHolder, int>("Resource");

    const char* src = R"(
        function createResource()
            local r = Resource(999)
            return r  -- Return extends lifetime
        end

        function createAndDiscard()
            local r = Resource(888)
            -- r dies here
        end

        kept = createResource()  -- Survives
        createAndDiscard()       -- Dies

        collectgarbage("collect")
    )";
    lua.loadAndExecuteScript(src);

    EXPECT_EQ(g_constructorCalls, 2);
    EXPECT_EQ(g_destructorCalls, 1); // Only 888 destroyed

    lua.loadAndExecuteScript("kept = nil; collectgarbage('collect')");
    EXPECT_EQ(g_destructorCalls, 2); // Now 999 also destroyed
}

TEST(DestructorTest, TableWithUserdata) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    State lua(State::LibBase);
    Metatable<ResourceHolder>::registerMetatable(lua);
    lua.binding.constructor<ResourceHolder, int>("Resource");

    const char* src = R"(
        objects = {
            a = Resource(1),
            b = Resource(2),
            c = Resource(3)
        }

        -- Remove one
        objects.b = nil
        collectgarbage("collect")
    )";
    lua.loadAndExecuteScript(src);

    EXPECT_EQ(g_constructorCalls, 3);
    EXPECT_EQ(g_destructorCalls, 1); // b destroyed

    lua.loadAndExecuteScript("objects = nil; collectgarbage('collect')");
    EXPECT_EQ(g_destructorCalls, 3); // All destroyed
}

TEST(DestructorTest, ReplacingReferences) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    State lua(State::LibBase);
    Metatable<ResourceHolder>::registerMetatable(lua);
    lua.binding.constructor<ResourceHolder, int>("Resource");

    const char* src = R"(
        r = Resource(100)
        r = Resource(200)  -- Old 100 should be GC'd
        r = Resource(300)  -- Old 200 should be GC'd

        collectgarbage("collect")
    )";
    lua.loadAndExecuteScript(src);

    EXPECT_EQ(g_constructorCalls, 3);
    EXPECT_EQ(g_destructorCalls, 2); // 100 and 200 destroyed, 300 still alive

    lua.loadAndExecuteScript("r = nil; collectgarbage('collect')");
    EXPECT_EQ(g_destructorCalls, 3); // All destroyed
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(DestructorTest, EmptyState) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    {
        State lua(State::LibBase);
        Metatable<ResourceHolder>::registerMetatable(lua);
        lua.binding.constructor<ResourceHolder, int>("Resource");

        // Register but don't create any objects
        lua.loadAndExecuteScript("collectgarbage('collect')");

    } // Destroy state

    EXPECT_EQ(g_constructorCalls, 0);
    EXPECT_EQ(g_destructorCalls, 0);
}

TEST(DestructorTest, ManyObjects) {
    g_constructorCalls = 0;
    g_destructorCalls = 0;

    {
        State lua(State::LibBase);
        Metatable<ResourceHolder>::registerMetatable(lua);
        lua.binding.constructor<ResourceHolder, int>("Resource");

        const char* src = R"(
            objects = {}
            for i = 1, 100 do
                objects[i] = Resource(i)
            end
        )";
        lua.loadAndExecuteScript(src);

        EXPECT_EQ(g_constructorCalls, 100);
        EXPECT_EQ(g_destructorCalls, 0);

    } // All destroyed

    EXPECT_EQ(g_destructorCalls, 100);
}

} // namespace Lua
