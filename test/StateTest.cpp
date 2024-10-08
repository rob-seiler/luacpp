#include <gtest/gtest.h>
#include <string>

#ifdef USE_CPP20_MODULES
import luacpp.State;
import luacpp.TypeMismatchException;
#else
#include <luacpp/State.hpp>
#include <luacpp/TypeMismatchException.hpp>
#endif

//#include <lua/lua.hpp>
struct lua_State;

namespace Lua {

class TestObject {
public:
	TestObject() : m_x(0.0), m_y(0.0) { ++ObjectCount; }
	~TestObject() { --ObjectCount; }
	void translate(double x, double y) { m_x += x; m_y += y; }

	static uint32_t ObjectCount;
private:
	double m_x;
	double m_y;
};

uint32_t TestObject::ObjectCount = 0;

class Counter {
public:
	Counter() = default;
	void increment() { ++m_count; }
	void increase(int val) { m_count += val; }
	int getCount() const { return m_count; }
private:
	int m_count = 0; 
};

int destroyObject(lua_State* lvm) {
	State lua(lvm);
	TestObject* obj = reinterpret_cast<TestObject*>(lua.getArgument<void*>(1));
	obj->~TestObject(); //call destructor
	return 0;
};


class StateTest : public ::testing::Test {
public:
	StateTest() {
	}

	virtual ~StateTest() {
	}

protected:

};

using namespace std::literals::string_view_literals;


TEST_F(StateTest, simpleScriptExecution) {
	const char* src = R"(
		-- This is a Lua script
		x = 10 + 2
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0);
	EXPECT_EQ(script.getStackSize(), 0);
}

TEST_F(StateTest, simpleScriptWithInvalidSyntax) {
	const char* src = R"(
		-- This is invalid Lua syntax
		x = 10 +
	)";

	State script(State::LibNone);
	EXPECT_NE(script.loadAndExecuteScript(src), 0);
	EXPECT_EQ(script.getStackSize(), 0);
	EXPECT_FALSE(script.getErrorList().empty());
	for (const std::string& err : script.getErrorList()) {
		std::cout << err << std::endl;
	}
}

TEST_F(StateTest, readVariable) {
	const char* src = R"(
		-- This is a Lua script
		x = 10 + 2
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0);
	EXPECT_EQ(script.readVariable<int>("x"), 12);
}

TEST_F(StateTest, writeVariable) {
	const char* src = R"(
		-- This is a Lua script
		x = 0
		y = 0
		function calcY()
			y = x + 2
		end
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0);
	EXPECT_EQ(script.executeFunction("calcY"), 0);
	EXPECT_EQ(script.readVariable<int>("y"), 2);

	script.writeVariable("x", 10);
	EXPECT_EQ(script.executeFunction("calcY"), 0);
	EXPECT_EQ(script.readVariable<int>("y"), 12);
}

TEST_F(StateTest, executeScriptFromRegistry) {
	constexpr static const char* const ScriptKey = "test";
	const char* src = R"(
		-- This is a Lua script
		x = x + 1
	)";

	//load the script into the registry
	State script(State::LibNone);
	ASSERT_EQ(script.loadScript(ScriptKey, src), 0);
	EXPECT_EQ(script.getStackSize(), 0);

	//execute the script
	script.writeVariable("x", 0);
	EXPECT_EQ(script.executeScript(ScriptKey), 0);
	EXPECT_EQ(script.getStackSize(), 0);
	EXPECT_EQ(script.readVariable<int>("x"), 1);

	//execute the script again
	EXPECT_EQ(script.executeScript(ScriptKey), 0);
	EXPECT_EQ(script.getStackSize(), 0);
	EXPECT_EQ(script.readVariable<int>("x"), 2);
}

TEST_F(StateTest, simpleFunctionWithReturnValue) {
	const char* src = R"(
		function sqr(x)
			return x * x
		end

		function calcHypothenuse(a, b)
			return math.sqrt(sqr(a) + sqr(b))
		end
	)";

	State script(State::LibMath);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	int rc = 0;
	EXPECT_EQ(script.executeFunctionAndReadReturnVal(rc, "calcHypothenuse", 3, 4), 0);
	EXPECT_EQ(rc, 5);
}

TEST_F(StateTest, simpleNativeFunction) {
	//this time we don't define sqr in the script, but in C++
	const char* src = R"(
		function calcHypothenuse(a, b)
			return math.sqrt(sqr(a) + sqr(b))
		end
	)";
	auto sqr = [](lua_State* lvm) -> int {
		State lua(lvm);
		double val = lua.getArgument<double>(1);
		return lua.setReturnValue(val * val);
	};

	State script(State::LibMath);
	EXPECT_EQ(script.registerNativeFunction("sqr", sqr), 0);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	int rc = 0;
	EXPECT_EQ(script.executeFunctionAndReadReturnVal(rc, "calcHypothenuse", 3, 4), 0);
	EXPECT_EQ(rc, 5);
	EXPECT_EQ(script.getStackSize(), 0);
}

TEST_F(StateTest, registerMethod) {
	const char* src = R"(
		count(10);
	)";

	Counter counter;
	State script(State::LibNone);
	EXPECT_EQ(script.registerMethod("count", [&counter](State& script) {
		const int count = script.getArgument<int>(1);
		counter.increase(count);
		return 0;
	}), 0);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0);
	EXPECT_EQ(counter.getCount(), 10);
}

TEST_F(StateTest, registerDebugHook) {
	const char* src = R"(
		x = 10
		y = 20
		z = x + y
	)";

	State script(State::LibNone);
	uint32_t callCount = 0;
	script.registerDebugHook([&callCount](State& script, const DebugInfo& info) {
		++callCount;
		EXPECT_EQ(info.event, static_cast<int>(EventCodes::Line));
		EXPECT_EQ(info.currentline, callCount + 1); //we have a new line right after the raw string starts
	}, MaskLine, 0);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0);
	EXPECT_EQ(callCount, 3);
}

TEST_F(StateTest, readTable) {
	const char* src = R"(
		map = { a = 1, b = 2, c = 3	}
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	auto map = script.readTable<std::string, int>("map");
	EXPECT_EQ(map.size(), 3);
	EXPECT_EQ(map["a"], 1);
	EXPECT_EQ(map["b"], 2);
	EXPECT_EQ(map["c"], 3);
}

TEST_F(StateTest, readTable_invalidValueType) {
	const char* src = R"(
		map = { a = 1, b = "two", c = 3	}
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope

	bool exceptionRaised = false;
	try {
		script.readTable<std::string,int>("map");
	} catch (const TypeMismatchException& e) {
		EXPECT_EQ(e.getExpectedType(), Type::Number);
		EXPECT_EQ(e.getActualType(), Type::String);
		exceptionRaised = true;
	} catch (...) {
		//unexpected exception raised
	}
	EXPECT_TRUE(exceptionRaised);
	EXPECT_EQ(script.getStackSize(), 0);
}

TEST_F(StateTest, readTableIfMatching) {
	const char* src = R"(
		map = { a = 1, b = "two", c = 3	}
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	auto map = script.readTableIfMatching<std::string, int>("map");
	EXPECT_EQ(map.size(), 2);
	EXPECT_EQ(map["a"], 1);
	EXPECT_EQ(map["c"], 3);
}

TEST_F(StateTest, readTableGeneric) {
	const char* src = R"(
		map = { a = 1, b = "two", c = 3.0 }
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	auto map = script.readTableGeneric("map");
	EXPECT_EQ(map.size(), 3);

	EXPECT_EQ(map["a"], Generic(1ll));
	EXPECT_EQ(map["b"], Generic(std::string("two")));
	EXPECT_EQ(map["c"], Generic(3.0));
}

TEST_F(StateTest, writeTable) {
	const char* src = R"(
		-- This is a Lua script
		y = 0
		function calcY()
			y = map.a + map.b
		end
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0);
	std::map<std::string, int> map = { { "a", 10 }, { "b", 20 } };
	script.writeTable("map", map);
	
	EXPECT_EQ(script.executeFunction("calcY"), 0);
	EXPECT_EQ(script.readVariable<int>("y"), 30);
}

TEST_F(StateTest, withTableDo) {
	const char* src = R"(
		map = { a = 1, b = 2, c = 3	}
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	
	int a = 0, b = 0, c = 0;
	script.withTableDo("map", [&a, &b, &c](Table& table) {
		EXPECT_TRUE(table.readValue<int>("a", a));
		EXPECT_TRUE(table.readValue<int>("b", b));
		EXPECT_TRUE(table.readValue<int>("c", c));
	}, false);

	EXPECT_EQ(script.getStackSize(), 0); //all requested values should be popped from the stack
	EXPECT_EQ(a, 1);
	EXPECT_EQ(b, 2);
	EXPECT_EQ(c, 3);
}

TEST_F(StateTest, nestedTable) {
	const char* src = R"(
		map = { a = 1, b = 2, c = { d = 3, e = 4 } }
	)";

	State script(State::LibNone);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	
	int a = 0, b = 0, d = 0, e = 0;
	script.withTableDo("map", [&a, &b, &d, &e](Table& table) {
		EXPECT_TRUE(table.readValue<int>("a", a));
		EXPECT_TRUE(table.readValue<int>("b", b));
		table.withTableDo("c", [&d, &e](Table& table) {
			EXPECT_TRUE(table.readValue<int>("d", d));
			EXPECT_TRUE(table.readValue<int>("e", e));
		});
	}, false);

	EXPECT_EQ(script.getStackSize(), 0); //all requested values should be popped from the stack
	EXPECT_EQ(a, 1);
	EXPECT_EQ(b, 2);
	EXPECT_EQ(d, 3);
	EXPECT_EQ(e, 4);

}

TEST_F(StateTest, metatable) {
	constexpr static const char* const MetaTable = "Vec2MetaTable";
	const char* src = R"(
		v1 = createVector() -- createVector is a native function which returns a table
		v2 = createVector()
		v1.x = 10
		v1.y = 20
		v2.x = 1
		v2.y = 2
		v3 = v1 + v2 -- this should call the __add metamethod
		print("v3: " .. v3.x .. ", " .. v3.y)
	)";

	State script(State::LibBase);
	struct Vec2 {
		static void createVectorTable(State& lua, double x, double y) {
			//we want to keep the table on the stack because it is the return value
			lua.createTable(nullptr, [x, y](Table& table) {
				table.setElement("x", x);
				table.setElement("y", y);
				table.assignMetaTable(MetaTable); //assign the metatable we've previously created to the table
			});
		}

		static int create(lua_State* lvm) {
			State lua(lvm);
			createVectorTable(lua, 0, 0);			
			return 1;
		}

		static int add(lua_State* lvm) {
			State lua(lvm);
			double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
			if (lua.getStackSize() >= 2) {
				//read 1st operand
				lua.withTableDo(1, [&x1, &y1](Table& table) {
					table.readValue<double>("x", x1);
					table.readValue<double>("y", y1);
				});
				//read 2nd operand
				lua.withTableDo(2, [&x2, &y2](Table& table) {
					table.readValue<double>("x", x2);
					table.readValue<double>("y", y2);
				});
				//pop the operands from the stack
				lua.popStack(2);
			}
			
			createVectorTable(lua, x1 + x2, y1 + y2);
			return 1;
		}
		double x;
		double y;
	};

	//add a meta table which defines the __add metamethod for our vector
	script.createMetaTable(MetaTable, [](Table& table) {
		table.setElement(State::MetaTable::Addition, Vec2::add);
	});

	script.registerNativeFunction("createVector", Vec2::create);
	EXPECT_EQ(script.loadAndExecuteScript(src), 0); //we need to execute the script once to get the functions into the global scope
	for (const std::string& err : script.getErrorList()) {
		std::cout << err << std::endl;
	}

	//read out v3 to check against
	double v3x = 0, v3y = 0;
	script.withTableDo("v3", [&v3x, &v3y](Table& table) {
		EXPECT_TRUE(table.readValue<double>("x", v3x));
		EXPECT_TRUE(table.readValue<double>("y", v3y));
	}, false);

	EXPECT_EQ(script.getStackSize(), 0); //all requested values should be popped from the stack
	EXPECT_DOUBLE_EQ(v3x, 11.0);
	EXPECT_DOUBLE_EQ(v3y, 22.0);
}

TEST_F(StateTest, ctordtor) {
	constexpr static const char* const MetaTable = "TestObjectMetaTable";
	const char* src = R"(
		obj = createObject()
		translate(obj, 10, 20)
	)";

	{
		State script(State::LibBase);
		//register functions
		script.registerNativeFunction("createObject", [](lua_State* lvm) -> int {
			State lua(lvm);
			TestObject* obj = lua.createUserData<TestObject>();
			lua.assignMetaTable(MetaTable); //assign the meta table to the userdata
			return 1;
		});
		script.registerNativeFunction("translate", [](lua_State* lvm) -> int {
			State lua(lvm);
			TestObject* obj = reinterpret_cast<TestObject*>(lua.getArgument<void*>(1));
			double x = lua.getArgument<double>(2);
			double y = lua.getArgument<double>(3);
			obj->translate(x, y);
			return 0;
		});
		//register metatable
		script.createMetaTable(MetaTable, [](Table& table) {
			table.setElement(State::MetaTable::GC, destroyObject);
		});

		EXPECT_EQ(script.loadAndExecuteScript(src), 0);
		EXPECT_EQ(TestObject::ObjectCount, 1);
	}
	EXPECT_EQ(TestObject::ObjectCount, 0);
}

} // namespace Lua