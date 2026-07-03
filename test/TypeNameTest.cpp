#include <gtest/gtest.h>

#include <luacpp/Bind.hpp> // binding.constructor bodies live here
#include <luacpp/Metatable.hpp>
#include <luacpp/State.hpp>
#include <luacpp/detail/TypeName.hpp>

#include <lua/lua.hpp>

#include <string>
#include <string_view>

// Deliberately a NAMED namespace at file scope: anonymous namespaces are
// spelled differently per compiler ("(anonymous namespace)", "{anonymous}",
// "`anonymous-namespace'"), so only named types have cross-compiler-exact
// expected strings.
namespace LuaTypeNameTest {

struct Widget {
	float x = 0.0f;
};

class Gadget {
public:
	int v = 0;
};

enum class Color { Red, Green };

struct Outer {
	struct Inner {
		int i = 0;
	};
};

namespace nested {
struct Deep {
	int d = 0;
};
} // namespace nested

template <typename T>
struct Box {
	T value;
};

} // namespace LuaTypeNameTest

namespace Lua {
namespace {

#if LUACPP_HAS_STABLE_TYPENAME

// Compile-time usability is part of the contract.
static_assert(detail::typeName<int>() == std::string_view("int"));
static_assert(detail::typeName<LuaTypeNameTest::Widget>() ==
              std::string_view("LuaTypeNameTest::Widget"));

TEST(TypeNameTest, plainStructYieldsQualifiedName) {
	EXPECT_EQ(detail::typeName<LuaTypeNameTest::Widget>(),
	          std::string_view("LuaTypeNameTest::Widget"));
}

TEST(TypeNameTest, classKeywordIsStripped) {
	EXPECT_EQ(detail::typeName<LuaTypeNameTest::Gadget>(),
	          std::string_view("LuaTypeNameTest::Gadget"));
}

TEST(TypeNameTest, enumClassKeywordIsStripped) {
	EXPECT_EQ(detail::typeName<LuaTypeNameTest::Color>(),
	          std::string_view("LuaTypeNameTest::Color"));
}

TEST(TypeNameTest, nestedTypeAndNamespaceAreQualified) {
	EXPECT_EQ(detail::typeName<LuaTypeNameTest::Outer::Inner>(),
	          std::string_view("LuaTypeNameTest::Outer::Inner"));
	EXPECT_EQ(detail::typeName<LuaTypeNameTest::nested::Deep>(),
	          std::string_view("LuaTypeNameTest::nested::Deep"));
}

TEST(TypeNameTest, metatableNameIsPrefixedAndNulTerminated) {
	// EXPECT_STREQ walks up to the terminator — validates both the prefix
	// and the compile-time NUL termination of the storage.
	EXPECT_STREQ(Metatable<LuaTypeNameTest::Widget>::metatableName(),
	             "luacpp.LuaTypeNameTest::Widget");
}

TEST(TypeNameTest, registryKeyMatchesExpectedName) {
	State lua(State::LibBase);
	Metatable<LuaTypeNameTest::Widget>::registerMetatable(lua);

	// The documented, cross-compiler-stable registry key must actually be
	// the one under which the metatable was registered.
	lua_State* L = lua.getState();
	EXPECT_EQ(luaL_getmetatable(L, "luacpp.LuaTypeNameTest::Widget"), LUA_TTABLE);
	lua_pop(L, 1);
}

#endif // LUACPP_HAS_STABLE_TYPENAME

TEST(TypeNameTest, templateInstantiationsAreDeterministicPerCompiler) {
	// Documented non-guarantee: template names differ across compilers
	// (default arguments, spacing) but must be non-empty, self-consistent
	// and distinct from other instantiations.
	const char* a = Metatable<LuaTypeNameTest::Box<int>>::metatableName();
	const char* b = Metatable<LuaTypeNameTest::Box<float>>::metatableName();
	ASSERT_NE(a, nullptr);
	EXPECT_NE(std::string_view(a), std::string_view(b));
	EXPECT_NE(std::string_view(a).find("Box"), std::string_view::npos);
	EXPECT_STREQ(a, Metatable<LuaTypeNameTest::Box<int>>::metatableName());
}

TEST(TypeNameTest, distinctTypesGetDistinctNames) {
	EXPECT_NE(std::string_view(Metatable<LuaTypeNameTest::Widget>::metatableName()),
	          std::string_view(Metatable<LuaTypeNameTest::Gadget>::metatableName()));
}

TEST(TypeNameTest, tostringUsesTheStableName) {
	State lua(State::LibBase);
	Metatable<LuaTypeNameTest::Widget>::registerMetatable(lua);
	lua.binding.constructor<LuaTypeNameTest::Widget>("Widget");

	// Widget has no toString(), so Lua's default "<__name>: <address>"
	// fallback surfaces the registered metatable name.
	lua.loadAndExecuteScript("w = Widget(); result = tostring(w)");

	const auto result = lua.variables.read<std::string>("result");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->rfind("luacpp.", 0), 0u) << *result;
}

} // namespace
} // namespace Lua
