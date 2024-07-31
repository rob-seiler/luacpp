#include <gtest/gtest.h>

#ifdef USE_CPP20_MODULES
import luacpp.Registry;
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

TEST_F(RegistryTest, setValue) {
	m_registry.setEntry("test", 42);
	EXPECT_EQ(m_registry.getEntry("test"), Type::Number);
	EXPECT_EQ(Basics::getStackValue<int>(m_state, -1), 42);

	m_registry.setEntry(42, "test");
	EXPECT_EQ(m_registry.getEntry(42), Type::String);
	EXPECT_EQ(Basics::getStackValue<std::string>(m_state, -1), "test");

	m_registry.setEntry(42.0, true);
	EXPECT_EQ(m_registry.getEntry(42.0), Type::Boolean);
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


} // namespace Lua