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
