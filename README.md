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
After building the `luacpp` library, you can use it in your C++ projects to interact with Lua.

Here's a basic example of how to use `luacpp` to execute a Lua script:

```cpp
#include <lua/State.hpp>

int main() {
	const char* src = "x = 10 + 2";
	Lua::State lua;

	lua.loadAndExecuteScript(src);
	const int x = lua.readVariable<int>("x");
	return 0;
}
```
In this example, we first include the State.hpp header. Then we define a Lua script as a string. This script simply assigns the value 10 + 2 to the variable x. We create a Lua::State object, which represents a Lua state. We then load and execute the Lua script using the `loadAndExecuteScript` method.

After the script is executed, we read the value of the variable x from the Lua state using the readVariable method. The `readVariable` method is templated, so we specify int as the template argument to indicate that we expect x to be an integer. The value of x is then stored in the x variable in our C++ code.

The method `loadAndExecuteScript` expects the source code as a string. If you want to load the script from a file, you can use the simple literal `_load` provided in the file `Literal.hpp`. This literal is a very simple implemenation using `ifstream`. Here is how you would modify the code above to load the script from a file:
```c++
lua.loadAndExecuteScript("path/to/myscript.lua"_load);
```
