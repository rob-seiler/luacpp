#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Metatable.hpp>

#include <cmath>

namespace Lua {

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

TEST(MethodRegistryTest, PrimitiveReturn) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::length>("length");

	const char* src = "v = Vec(3, 4); result = v:length()";
	EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

	float result = static_cast<float>(lua.readVariable<double>("result"));
	EXPECT_FLOAT_EQ(result, 5.0f);
}

TEST(MethodRegistryTest, UserdataReturn) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::scaled>("scaled");

	const char* src = "v = Vec(2, 3); result = v:scaled(2.5)";
	EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

	Vec* result = lua.readVariable<Vec*>("result");
	ASSERT_NE(result, nullptr);
	EXPECT_FLOAT_EQ(result->x, 5.0f);
	EXPECT_FLOAT_EQ(result->y, 7.5f);
}

TEST(MethodRegistryTest, UserdataArg) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::dot>("dot");

	const char* src = "v1 = Vec(3, 4); v2 = Vec(1, 2); result = v1:dot(v2)";
	EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

	float result = static_cast<float>(lua.readVariable<double>("result"));
	EXPECT_FLOAT_EQ(result, 11.0f);
}

TEST(MethodRegistryTest, VoidReturn) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::reset>("reset");

	const char* src = "v = Vec(7, 8); v:reset()";
	EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

	Vec* v = lua.readVariable<Vec*>("v");
	ASSERT_NE(v, nullptr);
	EXPECT_FLOAT_EQ(v->x, 0.0f);
	EXPECT_FLOAT_EQ(v->y, 0.0f);
}

TEST(MethodRegistryTest, MultipleArgs) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::count>("count");

	const char* src = "v = Vec(0, 0); result = v:count(1, 2, 3)";
	EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

	int result = lua.readVariable<int>("result");
	EXPECT_EQ(result, 6);
}

TEST(MethodRegistryTest, WrongSelfType_Errors) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	Metatable<Other>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.registerConstructor<Other, int>("Other");
	lua.bindMethod<Vec, &Vec::length>("length");

	const char* src = "o = Other(42); result = Vec.length(o)";
	int status = lua.loadAndExecuteScript(src);
	EXPECT_NE(status, 0);
}

TEST(MethodRegistryTest, WrongArgType_Errors) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::dot>("dot");

	const char* src = "v = Vec(1, 2); result = v:dot(42)";
	int status = lua.loadAndExecuteScript(src);
	EXPECT_NE(status, 0);
}

TEST(MethodRegistryTest, MethodAndOperatorsCoexist) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.bindMethod<Vec, &Vec::length>("length");

	const char* src = "v1 = Vec(3, 0); v2 = Vec(0, 4); result = (v1 + v2):length()";
	EXPECT_EQ(lua.loadAndExecuteScript(src), 0);

	float result = static_cast<float>(lua.readVariable<double>("result"));
	EXPECT_FLOAT_EQ(result, 5.0f);
}

TEST(MethodRegistryTest, MultipleMethods) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
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

TEST(MethodRegistryTest, MethodOnDifferentTypes) {
	State lua(State::LibBase);
	Metatable<Vec>::registerMetatable(lua);
	Metatable<Other>::registerMetatable(lua);
	lua.registerConstructor<Vec, float, float>("Vec");
	lua.registerConstructor<Other, int>("Other");
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

} // namespace Lua
