#include <gtest/gtest.h>

#ifdef USE_CPP20_MODULES
import luacpp.Registry;
import luacpp.Basics;
#else
#include <luacpp/Registry.hpp>
#endif

#include <lua/lua.hpp>


namespace Lua {

class RegistryTest : public ::testing::Test {
public:
	RegistryTest() : m_state(luaL_newstate()), m_registry(m_state) {
		luaL_openlibs(m_state);
	}
protected:
	lua_State* m_state;
	Registry m_registry;
};

TEST_F(RegistryTest, setElement_getElement) {
	m_registry.setElement("test", 42);
	m_registry.setElement(42, "test");
	m_registry.setElement(43.0, true);

	EXPECT_EQ(m_registry.getElement("test"), Type::Number);
	EXPECT_EQ(Basics::getStackValue<int>(m_state, -1), 42);
	
	EXPECT_EQ(m_registry.getElement(42), Type::String);
	EXPECT_EQ(Basics::getStackValue<std::string>(m_state, -1), "test");
	
	EXPECT_EQ(m_registry.getElement(43.0), Type::Boolean);
	EXPECT_EQ(Basics::getStackValue<bool>(m_state, -1), true);
}

TEST_F(RegistryTest, loadScript) {
	const char* script = "print(\"Hello, World!\")";
	Registry::ErrorCode res = m_registry.loadScript(1, script);
	ASSERT_EQ(res, Registry::ErrorCode::Ok);

	res = m_registry.getScript(1);
	ASSERT_EQ(res, Registry::ErrorCode::Ok);

	//just test if the function gets executed
	EXPECT_EQ(lua_pcall(m_state, 0, 0, 0), 0);
}

TEST_F(RegistryTest, loadScript_invalidSyntax) {
	const char* script = "print(\"Hello, World!\"";
	Registry::ErrorCode res = m_registry.loadScript(1, script);
	EXPECT_EQ(res, Registry::ErrorCode::SyntaxError);

	res = m_registry.getScript(1);
	EXPECT_EQ(res, Registry::ErrorCode::RuntimeError);
}

TEST_F(RegistryTest, copyContent) {
	constexpr const char* test = "test";
	m_registry.setElement("test", 42);
	m_registry.setElement(42, "test");
	m_registry.setElement(43.0, true);

	lua_State* otherState = luaL_newstate();
	Registry cpy(otherState);
	cpy.copyContent(m_registry);

	EXPECT_EQ(m_registry.getElement("test"), Type::Number);
	EXPECT_EQ(Basics::getStackValue<int>(m_state, -1), 42);
	EXPECT_EQ(cpy.getElement("test"), Type::Number);
	EXPECT_EQ(Basics::getStackValue<int>(otherState, -1), 42);

	EXPECT_EQ(m_registry.getElement(42), Type::String);
	EXPECT_EQ(Basics::getStackValue<std::string>(m_state, -1), "test");
	EXPECT_EQ(cpy.getElement(42), Type::String);
	EXPECT_EQ(Basics::getStackValue<std::string>(otherState, -1), "test");

	EXPECT_EQ(m_registry.getElement(43.0), Type::Boolean);
	EXPECT_EQ(Basics::getStackValue<bool>(m_state, -1), true);
	EXPECT_EQ(cpy.getElement(43.0), Type::Boolean);
	EXPECT_EQ(Basics::getStackValue<bool>(otherState, -1), true);

	lua_close(otherState);
}

} // namespace Lua