#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/TypeMismatchException.hpp>
#include <lua/lua.hpp>

#include "TestSupport.hpp"

#include <array>
#include <memory>
#include <stdexcept>
#if LUACPP_HAS_SPAN
#include <span>
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

// Regression guard: State is intentionally non-copyable and non-movable.
// registerMethod() captures `this` inside Lua C-closures, so any move would
// leave previously registered closures pointing at a stale object. Wrap in
// std::unique_ptr<State> if you need transfer of ownership.
static_assert(!std::is_copy_constructible_v<State>,
              "State must not be copy-constructible");
static_assert(!std::is_move_constructible_v<State>,
              "State must not be move-constructible");
static_assert(!std::is_copy_assignable_v<State>,
              "State must not be copy-assignable");
static_assert(!std::is_move_assignable_v<State>,
              "State must not be move-assignable");


TEST_F(StateTest, simpleScriptExecution) {
	const char* src = R"(
		-- This is a Lua script
		x = 10 + 2
	)";

	State script(State::LibNone);
	script.loadAndExecuteScript(src);
	EXPECT_EQ(script.getStackSize(), 0);
}

TEST_F(StateTest, simpleScriptWithInvalidSyntax) {
	const char* src = R"(
		-- This is invalid Lua syntax
		x = 10 +
	)";

	State script(State::LibNone);
	auto& errors = script.installLogger<MemoryLogger>();
	script.loadAndExecuteScript(src);
	EXPECT_EQ(script.getStackSize(), 0);
	ASSERT_FALSE(errors.entries().empty());
	EXPECT_EQ(errors.entries().front().category, LuaError::Category::Load);
}

TEST_F(StateTest, readVariable) {
	const char* src = R"(
		-- This is a Lua script
		x = 10 + 2
	)";

	State script(State::LibNone);
	script.loadAndExecuteScript(src);
	EXPECT_EQ(readVar<int>(script, "x"), 12);
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
	script.loadAndExecuteScript(src);
	script.executeFunction("calcY");
	EXPECT_EQ(readVar<int>(script, "y"), 2);

	script.writeVariable("x", 10);
	script.executeFunction("calcY");
	EXPECT_EQ(readVar<int>(script, "y"), 12);
}

TEST_F(StateTest, executeScriptFromRegistry) {
	constexpr static const char* const ScriptKey = "test";
	const char* src = R"(
		-- This is a Lua script
		x = x + 1
	)";

	//load the script into the registry
	State script(State::LibNone);
	script.loadScript(ScriptKey, src);
	EXPECT_EQ(script.getStackSize(), 0);

	//execute the script
	script.writeVariable("x", 0);
	script.executeScript(ScriptKey);
	EXPECT_EQ(script.getStackSize(), 0);
	EXPECT_EQ(readVar<int>(script, "x"), 1);

	//execute the script again
	script.executeScript(ScriptKey);
	EXPECT_EQ(script.getStackSize(), 0);
	EXPECT_EQ(readVar<int>(script, "x"), 2);
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
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope
	auto rc = script.executeFunctionReturning<int>("calcHypothenuse", 3, 4);
	ASSERT_TRUE(rc.has_value());
	EXPECT_EQ(*rc, 5);
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
	script.registerNativeFunction("sqr", sqr);
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope
	auto rc = script.executeFunctionReturning<int>("calcHypothenuse", 3, 4);
	ASSERT_TRUE(rc.has_value());
	EXPECT_EQ(*rc, 5);
	EXPECT_EQ(script.getStackSize(), 0);
}

namespace {
constexpr const char* kSumAllSrc = R"(
	function sumAll(...)
		local s = 0
		for _, v in ipairs({...}) do s = s + v end
		return s
	end
)";
} // namespace

// Baseline coverage for the (pointer, length) overload — previously untested.
TEST_F(StateTest, executeFunctionWithArgsArrayPointerLength) {
	State script(State::LibBase); // ipairs lives in the base library
	script.loadAndExecuteScript(kSumAllSrc);

	const int args[] = {1, 2, 3, 4, 5};
	auto rc = script.executeFunctionWithArgsArrayReturning<int>("sumAll", args, 5);
	ASSERT_TRUE(rc.has_value());
	EXPECT_EQ(*rc, 15);
	EXPECT_EQ(script.getStackSize(), 0);
}

#if LUACPP_HAS_SPAN
// The std::span overload must produce the same result as (pointer, length),
// and must accept a span of const elements (read-only argument range).
TEST_F(StateTest, executeFunctionWithArgsArraySpan) {
	State script(State::LibBase); // ipairs lives in the base library
	script.loadAndExecuteScript(kSumAllSrc);

	const std::array<int, 5> args{1, 2, 3, 4, 5};

	// Status-only overload.
	EXPECT_EQ(script.executeFunctionWithArgsArray("sumAll", std::span(args)),
	          LuaError::Status::Ok);
	EXPECT_EQ(script.getStackSize(), 0);

	// Returning overload — span<const int> deduces a non-const optional<int>.
	auto rc = script.executeFunctionWithArgsArrayReturning("sumAll", std::span(args));
	ASSERT_TRUE(rc.has_value());
	EXPECT_EQ(*rc, 15);
	static_assert(std::is_same_v<decltype(rc), std::optional<int>>,
	              "span<const int> overload must strip const from the result type");
	EXPECT_EQ(script.getStackSize(), 0);
}
#endif // LUACPP_HAS_SPAN

// Both load-and-execute and execute paths must invoke the configured error
// handler so callers can inspect failures uniformly.
TEST_F(StateTest, executeScriptRecordsErrorOnFailure) {
	constexpr static const char* const ScriptKey = "errscript";
	// LibBase is required so error() resolves at runtime.
	const char* src = R"(
		error("boom from executeScript")
	)";

	State script(State::LibBase);
	auto& errors = script.installLogger<MemoryLogger>();
	script.loadScript(ScriptKey, src);
	ASSERT_TRUE(errors.entries().empty());

	script.executeScript(ScriptKey);
	EXPECT_EQ(script.getStackSize(), 0); // error was drained, not left dangling
	ASSERT_FALSE(errors.entries().empty());
	EXPECT_EQ(errors.entries().front().category, LuaError::Category::Runtime);
	EXPECT_NE(errors.entries().front().message.find("boom from executeScript"),
	          std::string::npos);
}

// executeScript<Generic> must preserve Registry::getScript's status: an
// unsupported key type (InvalidKey) is distinct from a missing/non-function
// key (RegistryKeyNotFound), and the caller should see which one happened.
TEST_F(StateTest, executeScriptWithInvalidGenericKeyPreservesStatus) {
	State script(State::LibBase);
	auto& errors = script.installLogger<MemoryLogger>();

	// Generic(nullptr) constructs a Generic of Type::Nil — getScript's switch
	// hits the default arm and returns InvalidKey.
	const auto rc = script.executeScript(Generic(nullptr));
	EXPECT_EQ(rc, LuaError::Status::InvalidKey)
	    << "executeScript must not flatten InvalidKey to RegistryKeyNotFound";
	ASSERT_FALSE(errors.entries().empty());
	EXPECT_EQ(errors.entries().front().status, LuaError::Status::InvalidKey);
}

// Errors raised with a non-string value (e.g. error({...}) propagates a
// table) must still be consumed from the Lua stack — otherwise the stack
// drifts on every subsequent call.
TEST_F(StateTest, tableErrorIsStringifiedAndStackStaysBalanced) {
	State script(State::LibBase);
	auto& errors = script.installLogger<MemoryLogger>();
	script.loadAndExecuteScript("error({code = 42, reason = 'boom'})");
	EXPECT_EQ(script.getStackSize(), 0)
	    << "non-string error must still be consumed from the stack";
	ASSERT_FALSE(errors.entries().empty());
	EXPECT_FALSE(errors.entries().front().message.empty())
	    << "luaL_tolstring should produce a non-empty default representation";
}

// Regression / upside of switching to luaL_tolstring: custom error objects
// that define __tostring surface their human-readable form instead of the
// default "table: 0x..." identity.
TEST_F(StateTest, tableErrorUsesCustomTostring) {
	State script(State::LibBase);
	auto& errors = script.installLogger<MemoryLogger>();
	script.loadAndExecuteScript(R"(
		local e = setmetatable({reason = "kaboom"}, {
			__tostring = function(self) return "Custom: " .. self.reason end
		})
		error(e)
	)");
	EXPECT_EQ(script.getStackSize(), 0);
	ASSERT_FALSE(errors.entries().empty());
	EXPECT_NE(errors.entries().front().message.find("Custom: kaboom"),
	          std::string::npos)
	    << "luaL_tolstring should honor __tostring on the error object";
}

// executeFunctionReturning surfaces success vs failure via the underlying
// Status return. A non-string error (error({...})) thus produces nullopt
// cleanly instead of being mis-read as a T value left on the stack.
TEST_F(StateTest, executeFunctionReturningSurfacesFailureViaStatus) {
	State script(State::LibBase);
	auto& errors = script.installLogger<MemoryLogger>();
	script.loadAndExecuteScript("function bad() error({reason = 'boom'}) end");

	// Direct call: status surfaces the runtime error.
	EXPECT_EQ(script.executeFunction("bad"), LuaError::Status::RuntimeError);

	// Returning variant: nullopt, no garbage read from the (table) error.
	auto result = script.executeFunctionReturning<int>("bad");
	EXPECT_FALSE(result.has_value());

	EXPECT_EQ(script.getStackSize(), 0)
	    << "both calls must leave the stack balanced";
	ASSERT_EQ(errors.entries().size(), 2u);
	EXPECT_EQ(errors.entries()[0].status, LuaError::Status::RuntimeError);
	EXPECT_EQ(errors.entries()[1].status, LuaError::Status::RuntimeError);
}

TEST_F(StateTest, loadAndExecuteScriptSurfacesStatusOnSuccessAndFailure) {
	State script(State::LibNone);
	auto& errors = script.installLogger<MemoryLogger>();

	EXPECT_EQ(script.loadAndExecuteScript("x = 1"), LuaError::Status::Ok);
	EXPECT_TRUE(errors.entries().empty());

	EXPECT_EQ(script.loadAndExecuteScript("x ="), LuaError::Status::SyntaxError);
	ASSERT_FALSE(errors.entries().empty());
	EXPECT_EQ(errors.entries().front().status, LuaError::Status::SyntaxError);
}

TEST_F(StateTest, executeFunctionWithUnknownNameReportsSyntheticError) {
	State script(State::LibNone);
	auto& errors = script.installLogger<MemoryLogger>();
	script.executeFunction("doesNotExist");
	ASSERT_FALSE(errors.entries().empty());
	const auto& err = errors.entries().front();
	EXPECT_EQ(err.category, LuaError::Category::Runtime);
	EXPECT_EQ(err.status, LuaError::Status::FunctionNotFound)
	    << "synthetic 'not a function' must surface as the typed code";
	EXPECT_NE(err.message.find("doesNotExist"), std::string::npos);
	EXPECT_EQ(script.getStackSize(), 0);
}

// Compile-and-run coverage for getUpValue<T>() so the public API stays
// instantiable end-to-end.
TEST_F(StateTest, getUpValue) {
	const char* src = R"(
		result = multiplyByFactor(6)
	)";

	auto multiplyByFactor = [](lua_State* lvm) -> int {
		State lua(lvm);
		const int value  = lua.getArgument<int>(1);
		const int factor = lua.getUpValue<int>(1);
		return lua.setReturnValue(value * factor);
	};

	State script(State::LibNone);
	script.registerNativeFunctionWithUpvalues("multiplyByFactor",
	                                          multiplyByFactor, 7);
	script.loadAndExecuteScript(src);
	EXPECT_EQ(readVar<int>(script, "result"), 42);
}

TEST_F(StateTest, registerMethod) {
	const char* src = R"(
		count(10);
	)";

	Counter counter;
	State script(State::LibNone);
	script.registerMethod("count", [&counter](State& script) {
		const int count = script.getArgument<int>(1);
		counter.increase(count);
		return 0;
	});
	script.loadAndExecuteScript(src);
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
	script.loadAndExecuteScript(src);
	EXPECT_EQ(callCount, 3);
}

TEST_F(StateTest, registerMethodRejectedOnBorrowedState) {
	// A borrowed wrapper must not register methods: dispatchMethod captures
	// `this` and m_callbacks is instance-local, so the closure would dangle
	// once the wrapper dies. Only the VM-owning State may register.
	State owner(State::LibNone);
	State borrowed(owner.getState());
	EXPECT_THROW(
	    borrowed.registerMethod("nope", [](State&) { return 0; }),
	    std::logic_error);
}

TEST_F(StateTest, registerDebugHookRejectedOnBorrowedState) {
	// A borrowed wrapper must not register a debug hook: the s_debugHooks
	// entry is keyed by the lua_State and only an owning State's destructor
	// erases it, so a borrowed registration would leak and outlive the wrapper.
	State owner(State::LibNone);
	State borrowed(owner.getState());
	EXPECT_THROW(
	    borrowed.registerDebugHook([](State&, const DebugInfo&) {}, MaskLine, 0),
	    std::logic_error);
}

// The debug hook may capture references whose lifetime is tied to the main
// State (or its outer scope). When the main wrapper dies while a borrowed
// wrapper keeps the VM alive, the hook must be torn down — otherwise the
// next Lua event invokes the lambda with dangling captures.
TEST_F(StateTest, debugHookDetachedOnMainDestructionEvenIfVmSurvives) {
	int callCount = 0;
	auto main = std::make_unique<State>(State::LibNone);
	main->registerDebugHook(
	    [&callCount](State&, const DebugInfo&) { ++callCount; },
	    MaskLine, 0);

	State borrowed(main->getState());
	main.reset();   // main dies; VM lives via `borrowed`

	const int before = callCount;
	borrowed.loadAndExecuteScript("a = 1\nb = 2\nc = 3");
	EXPECT_EQ(callCount, before)
	    << "debug hook must be detached when its registering main dies, "
	       "even if the VM survives via other wrappers";
}

// The VM survives the original owning wrapper as long as any other State
// instance still references its context. The intrusive refCount on
// StateContext drives lua_close, so the actual last-to-die wrapper triggers
// it — regardless of which one was the original creator.
TEST_F(StateTest, vmStaysAliveWhileBorrowedReferencesExist) {
	auto owner = std::make_unique<State>(State::LibNone);
	owner->loadAndExecuteScript("greeting = 'hello'");
	lua_State* L = owner->getState();

	State borrowed(L);  // shares the context, ref-count becomes 2

	owner.reset();      // ref-count drops to 1; VM must still be alive

	// If lua_close had run, this would be UB; with ref-counted lifetime
	// the VM survives until `borrowed` itself goes out of scope.
	auto g = borrowed.readVariable<std::string>("greeting");
	ASSERT_TRUE(g.has_value());
	EXPECT_EQ(*g, "hello");
}

// Wrapping a user-created lua_State transfers ownership to luacpp: the
// wrapper's destructor calls lua_close on the VM, and the caller must NOT
// call lua_close themselves afterwards.
TEST_F(StateTest, wrappingRawLuaStateTransfersOwnership) {
	lua_State* L = luaL_newstate();
	ASSERT_NE(L, nullptr);

	{
		State wrapper(L);
		wrapper.loadAndExecuteScript("x = 42");
		auto x = wrapper.readVariable<int>("x");
		ASSERT_TRUE(x.has_value());
		EXPECT_EQ(*x, 42);
	}
	// wrapper destructor closes L. The lua_State is now invalid; reaching
	// this point without a crash proves the wrapper handled its own cleanup.
	SUCCEED();
}

TEST_F(StateTest, readTable) {
	const char* src = R"(
		map = { a = 1, b = 2, c = 3	}
	)";

	State script(State::LibNone);
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope
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
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope

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
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope
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
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope
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
	script.loadAndExecuteScript(src);
	std::map<std::string, int> map = { { "a", 10 }, { "b", 20 } };
	script.writeTable("map", map);

	script.executeFunction("calcY");
	EXPECT_EQ(readVar<int>(script, "y"), 30);
}

TEST_F(StateTest, withTableDo) {
	const char* src = R"(
		map = { a = 1, b = 2, c = 3	}
	)";

	State script(State::LibNone);
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope

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
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope

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
	script.loadAndExecuteScript(src); //we need to execute the script once to get the functions into the global scope

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

		script.loadAndExecuteScript(src);
		EXPECT_EQ(TestObject::ObjectCount, 1);
	}
	EXPECT_EQ(TestObject::ObjectCount, 0);
}

} // namespace Lua
