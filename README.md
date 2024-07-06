# luacpp
simple c++ wrapper for lua

## Why another library?
Well the answer is simple. I just wanted to. For me the best way to learn something is to do it on my own. I had to implement Lua for work and got the permission to publish my wrapper on github :)

## Building
### Prerequisites

Luacpp requires at least c++17. If your compiler supports c++20 and you have cmake with a version of at least 3.28 then you'll have the option to compile the library as c++20 modules.

If you want to build the unit tests, you'll also need to checkout the `googletest` submodule.

### Steps

1. Clone the repository, navigate to the project directory, and optionally checkout the `googletest` submodule if you want to build the unit tests:

```bash
git clone https://github.com/rob-seiler/luacpp.git
cd luacpp
git submodule update --init --recursive  # Optional, for unit tests
```

2. Generate the CMake files and build the `luacpp` and `luacpp_test` targets. The `-DCMAKE_CXX_STANDARD=20 -DUSE_CPP20_MODULES=ON` options are optional and only needed if you want to build with C++20 modules. Without these options, the project will be built with C++17:

```bash
mkdir build
cd build
cmake -DCMAKE_CXX_STANDARD=20 -DUSE_CPP20_MODULES=ON ..  # Optional, for C++20 modules
cmake --build . --target luacpp
cmake --build . --target luacpp_test
```

After these steps, you should have a built version of the `luacpp` library and the `luacpp_test` unit tests in the `build` directory.

## Usage
### Basics
After building the `luacpp` library, you can use it in your C++ projects to interact with Lua.

Here's a basic example of how to use `luacpp` to execute a Lua script:

```cpp
#include <luacpp/State.hpp>

int main() {
	const char* src = "x = 10 + 2";
	Lua::State lua;

	lua.loadAndExecuteScript(src);
	const int x = lua.readVariable<int>("x");
	return 0;
}
```
In this example, we first include the State.hpp header. Then we define a Lua script as a string. This script simply assigns the value 10 + 2 to the variable x. We create a Lua::State object, which represents a Lua state. The object is loaded with no additional builtin libraries. We then load and execute the Lua script using the `loadAndExecuteScript` method.

After the script is executed, we read the value of the variable x from the Lua state using the readVariable method. The `readVariable` method is templated, so we specify int as the template argument to indicate that we expect x to be an integer. The value of x is then stored in the x variable in our C++ code.

The method `loadAndExecuteScript` expects the source code as a string. If you want to load the script from a file, you can use the simple literal `_load` provided in the file `Literal.hpp`. This literal is a very simple implemenation using `ifstream`. Here is how you would modify the code above to load the script from a file:
```c++
lua.loadAndExecuteScript("path/to/myscript.lua"_load);
```
### Embedding own functions
Loading and executing a script doesn't provide that much benifit without providing own functions to enable your application to be modifyable at runtime. You have several options to embedd functions in your lua state object. For simple function you could use method `registerNativeFunction` which accepts the c-style callback.
A fitting example would be a sleep method which doesn't have any dependencies.
```c++
#include <luacpp/State.hpp>
#include <chrono>
#include <thread>

int sleep(lua_State* lvm) {
	Lua::State lua(lvm);
	const int sleepTime = lua.getArgument<int>(1);
	std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
	return 0;
}
int main() {
	const char* src = "sleep(2)";

	Lua::State lua;
	lua.registerNativeFunction("sleep", sleep);
	lua.loadAndExecuteScript(src);
	return 0;
}
```

You can also assign upvalues to your c-function which you can read out in your function body to get access to them during execution.
```c++
#include <luacpp/State.hpp>
#include <iostream>

struct User {
	const char* name;
};

int sayHello(lua_State* lvm) {
	Lua::State lua(lvm);
	const User* user = lua.getUpValue<const User*>(1);
	std::cout << "Hello " << user->name << std::endl;
	return 0;
}
int main() {
	const char* src = "sayHello()";

	User user{"John"};

	Lua::State lua;
	lua.registerNativeFunctionWithUpvalues("sayHello", sayHello, &user);
	lua.loadAndExecuteScript(src);
	return 0;
}
```

For more complex situations where you want to get access to your classes you could use the generic method `registerMethod` which requires a std::function as argument. Using this method you can provide lamda methods with capture or bind methods to specific instances.

```c++
#include <luacpp/State.hpp>

int main() {
	const char* src = R"(
		x = score()
		y = score()
	)";

	unsigned int value = 0;

	Lua::State lua;
	lua.registerMethod("score", [&value](Lua::State& lua) -> int { 
		return lua.setReturnValue(++value);
	});
	lua.loadAndExecuteScript(src);
	return 0;
}
```

### Reading/Writing values
Another way of interacting between Lua and your application is by reading an writing variables.
To pass variables from C++ to Lua you can use the method _writeVariable_.
```c++
	Lua::State state;
	state.writeVariable<double>("x", 3.1415);
```

In a similar way you can read variables from Lua using the method _readVariable_.
```c++
	Lua::State state;
	double x = state.readVariable<double>("x");
```

There are other methods for different situations like _getArgument_ for reading function arguments or _getUpValue_ to read values bound to a function.

A bit more special is reading and writing tables. Luacpp provides several ways in reading and writing a table. The easiest way is to use the method _readTable_ and _readTableIfMatching_. Both methods assume the table holds the same type for all its values. The difference is in the handling if this is not the case. While _readTable_ throws an exception _readTableIfMatching_ skips the entry.

```c++
	Lua::State state;
	try {
		std::map<std::string, double> values = state.readTable<std::string, double>("lut");
	} catch (const Lua::TypeMismatchException& e) {
		//handle exception
	}
```

A bit more generic is the version readGeneric which holds a small wrapper around the type. This could be useful if you don't care for the type and only want to convert the value into a string.

```c++
	Lua::State state;
	auto map = script.readTableGeneric("table");
	for (const auto& [key, value] : map) {
		std::cout << key.toString() << ": " << value.toString() << std::endl;
	}
```

For more complex tables (e.g. tables with nested tables) you can also use the method _withTableDo_. This methods creates a table object and calls the callback function so you can work on this object. After returning from this method the table is automatically removed from stack.

```c++
	int a; float b;	std::string c;
	Lua::State state;
	state.withTableDo("map", [&a, &b, &c](Lua::Table& table) {
		table.readValue<int>("a", a);
		table.readValue<float>("b", b);
		table.readValue<std::string>("c", c);
	}, false);
```