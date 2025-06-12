#include <gtest/gtest.h>
#include <string>

#ifdef USE_CPP20_MODULES
import luacpp.Generic;
import luacpp.State;
#else
#include <luacpp/Generic.hpp>
#include <luacpp/State.hpp>
#endif

#include <lua/lua.hpp>

namespace Lua {

class GenericTest : public ::testing::Test {
};

TEST_F(GenericTest, ctor) {
    Generic generic;
    EXPECT_EQ(Type::Nil, generic.getType());
    EXPECT_EQ("nil", generic.toString());
}

TEST_F(GenericTest, ctor_withValue) {
    Generic generic(42);
    EXPECT_EQ(Type::Number, generic.getType());
    EXPECT_EQ("42", generic.toString());
    EXPECT_EQ(42, generic.get<int>());
    EXPECT_EQ(Generic(), generic.get(42));

    Generic generic2(42.0);
    EXPECT_EQ(Type::Number, generic2.getType());
    EXPECT_DOUBLE_EQ(42.0, std::stod(generic2.toString()));
    EXPECT_EQ(42.0, generic2.get<double>());
    EXPECT_EQ(Generic(), generic2.get(42.0));

    Generic generic3(true);
    EXPECT_EQ(Type::Boolean, generic3.getType());
    EXPECT_EQ("true", generic3.toString());
    EXPECT_EQ(true, generic3.get<bool>());
    EXPECT_EQ(Generic(), generic3.get(true));

    Generic generic4(false);
    EXPECT_EQ(Type::Boolean, generic4.getType());
    EXPECT_EQ("false", generic4.toString());
    EXPECT_EQ(false, generic4.get<bool>());
    EXPECT_EQ(Generic(), generic4.get(false));

    Generic generic5("Hello, World!");
    EXPECT_EQ(Type::String, generic5.getType());
    EXPECT_EQ("Hello, World!", generic5.toString());
    EXPECT_EQ("Hello, World!", generic5.get<std::string>());
    EXPECT_EQ(Generic(), generic5.get("Hello, World!"));

    Generic generic6(nullptr);
    EXPECT_EQ(Type::Nil, generic6.getType());
    EXPECT_EQ("nil", generic6.toString());
    EXPECT_EQ(nullptr, generic6.get<std::nullptr_t>());
    EXPECT_EQ(Generic(), generic6.get(nullptr));

    Generic generic7((void*)0x12345678);
    EXPECT_EQ(Type::LightUserData, generic7.getType());
    EXPECT_EQ("lightuserdata", generic7.toString());
    EXPECT_EQ((void*)0x12345678, generic7.get<void*>());
    EXPECT_EQ(Generic(), generic7.get((void*)0x12345678));
}

TEST_F(GenericTest, ctor_table) {
    std::map<Generic, Generic> table;
    table[Generic(42)] = Generic(42.0);
    table["Hello"] = "Hello";
    table["World"] = "World";

    Generic generic(table);
    EXPECT_EQ(Type::Table, generic.getType());
    EXPECT_EQ(Generic(42.0), generic.get(42));
    EXPECT_EQ(Generic("Hello"), generic.get("Hello"));
    EXPECT_EQ(Generic("World"), generic.get("World"));
    
    auto cpy = generic.get<std::map<Generic, Generic>>();
    EXPECT_EQ(42.0, cpy[Generic(42)].get<double>());
    EXPECT_EQ("Hello", cpy["Hello"].get<std::string>());
    EXPECT_EQ("World", cpy["World"].get<std::string>());
}

TEST_F(GenericTest, get) {
    Generic generic(42);
    EXPECT_EQ(42, generic.get<int>());

    Generic generic2(42.0);
    EXPECT_EQ(42.0, generic2.get<double>());

    Generic generic3(true);
    EXPECT_EQ(true, generic3.get<bool>());

    Generic generic4(false);
    EXPECT_EQ(false, generic4.get<bool>());

    Generic generic5("Hello, World!");
    EXPECT_EQ("Hello, World!", generic5.get<std::string>());

    Generic generic6(nullptr);
    EXPECT_EQ(nullptr, generic6.get<std::nullptr_t>());

    Generic generic7((void*)0x12345678);
    EXPECT_EQ((void*)0x12345678, generic7.get<void*>());
}

TEST_F(GenericTest, equals) {
    Generic generic(42);
    Generic generic2(42);
    EXPECT_TRUE(generic == generic2);

    Generic generic3(42.0);
    EXPECT_FALSE(generic == generic3);

    Generic generic4("a");
    EXPECT_FALSE(generic == generic4);

    Generic generic5("a");
    EXPECT_TRUE(generic4 == generic5);

    Generic generic6(nullptr);
    Generic generic7(nullptr);
    EXPECT_TRUE(generic6 == generic7);
}

TEST_F(GenericTest, lessThan) {
    Generic generic(42);
    Generic generic2(42);
    EXPECT_FALSE(generic < generic2);

    Generic generic3(42.1);
    EXPECT_TRUE(generic < generic3);

    Generic generic4("a");
    EXPECT_TRUE(generic < generic4);

    Generic generic5("a");
    EXPECT_FALSE(generic4 < generic5);

    Generic generic6(nullptr);
    Generic generic7(nullptr);
    EXPECT_FALSE(generic6 < generic7);
}

TEST_F(GenericTest, fromStack_Integer) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = 42"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Number, generic.getType());
    EXPECT_EQ(42, generic.get<int>());
}

TEST_F(GenericTest, fromStack_Double) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = 42.0"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Number, generic.getType());
    EXPECT_EQ(42.0, generic.get<double>());
}

TEST_F(GenericTest, fromStack_String) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = 'Hello, World!'"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::String, generic.getType());
    EXPECT_EQ("Hello, World!", generic.get<std::string>());
}

TEST_F(GenericTest, fromStack_Boolean) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = true"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Boolean, generic.getType());
    EXPECT_EQ(true, generic.get<bool>());

    ASSERT_EQ(state.loadAndExecuteScript("a = false"), 0);

    lua_getglobal(state.getState(), "a");
    generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Boolean, generic.getType());
    EXPECT_EQ(false, generic.get<bool>());
}

TEST_F(GenericTest, fromStack_Nil) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = nil"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Nil, generic.getType());
    EXPECT_EQ(nullptr, generic.get<std::nullptr_t>());
}

TEST_F(GenericTest, fromStack_Table) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = { x = 1, y = 2, z = 3 }"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Table, generic.getType());
    EXPECT_EQ(Generic(1), generic.get("x"));
    EXPECT_EQ(Generic(2), generic.get("y"));
    EXPECT_EQ(Generic(3), generic.get("z"));
}

TEST_F(GenericTest, fromStack_TableNested) {
    State state;
    ASSERT_EQ(state.loadAndExecuteScript("a = { x = 1, y = { a = 1, b = 2 } }"), 0);

    lua_getglobal(state.getState(), "a");
    Generic generic = Generic::fromStack(-1, state);
    EXPECT_EQ(Type::Table, generic.getType());
    EXPECT_EQ(Generic(1), generic.get("x"));
    EXPECT_EQ(Generic(1), generic.get("y").get("a"));
    EXPECT_EQ(Generic(2), generic.get("y").get("b"));
}

} //namespace Lua