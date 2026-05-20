# C++20 module wrapper

luacpp ships a thin C++20 module that re-exports the public headers as a named module called `luacpp`. Consumers can then write:

```cpp
import luacpp;

int main() {
    Lua::State lua;
    lua.loadAndExecuteScript("x = 1 + 2");
    return 0;
}
```

instead of pulling in `<luacpp/State.hpp>`, `<luacpp/Metatable.hpp>` and friends individually. For large downstream codebases the module surface gives the usual C++20 module compile-time benefits without forcing luacpp to restructure its internals — the static library and its headers remain authoritative; the `.cppm` is just a wrapper.

## Building

The module is **opt-in**. The default build still produces only the static library against C++17:

```bash
cmake -DLUACPP_BUILD_MODULE=ON ..
cmake --build . --target luacpp_module
```

Requirements:

- CMake **3.28** or newer (for `FILE_SET CXX_MODULES`).
- A compiler with C++20 module support: Clang 16+, GCC 14+, MSVC 17.5+.

The base library still builds with CMake 3.10 and C++17 — turning on `LUACPP_BUILD_MODULE` raises the bar only for the module target itself.

## Consuming the module

In CMake:

```cmake
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE luacpp::module)
```

`luacpp::module` is an alias for the module target and transitively links the underlying static library, so a single `target_link_libraries` line wires up both the module surface and the runtime.

## What is exported

Everything in the `Lua::` namespace that consumers would normally reach through the public headers — `State`, `Table`, `Registry`, `Generic`, `Metatable<T>`, `Bind`, `Stack<T>`, `Version`, `Debug`, `TypeMismatchException`, the event-mask constants, `Type`, `toString(Type)`, the `Basics` helper.

The `_load` user-defined literal from `<luacpp/Literals.hpp>` is **not** re-exported through the module surface — `using` declarations of UDLs are inconsistently supported across module-aware compilers. If you need it, include `<luacpp/Literals.hpp>` alongside `import luacpp;` as a transitional step.

## When not to use the module

If your codebase is still mostly header-based, the include path stays the better fit — the module adds a build-system dependency (CMake 3.28+, module-capable compiler) without changing the API. The module is meant for projects that already invest in modules for their own code.
