#include <gtest/gtest.h>
#include <string>

#ifdef USE_CPP20_MODULES
import luacpp.Generic;
#else
#include <luacpp/Generic.hpp>
#endif

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

    Generic generic2(42.0);
    EXPECT_EQ(Type::Number, generic2.getType());
    EXPECT_DOUBLE_EQ(42.0, std::stod(generic2.toString()));

    Generic generic3(true);
    EXPECT_EQ(Type::Boolean, generic3.getType());
    EXPECT_EQ("true", generic3.toString());

    Generic generic4(false);
    EXPECT_EQ(Type::Boolean, generic4.getType());
    EXPECT_EQ("false", generic4.toString());

    Generic generic5("Hello, World!");
    EXPECT_EQ(Type::String, generic5.getType());
    EXPECT_EQ("Hello, World!", generic5.toString());

    Generic generic6(nullptr);
    EXPECT_EQ(Type::Nil, generic6.getType());
    EXPECT_EQ("nil", generic6.toString());

    Generic generic7((void*)0x12345678);
    EXPECT_EQ(Type::LightUserData, generic7.getType());
    EXPECT_EQ("lightuserdata", generic7.toString());
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

} //namespace Lua