[![Build and Test](https://github.com/rob-seiler/luacpp/actions/workflows/buildntest.yml/badge.svg)](https://github.com/rob-seiler/luacpp/actions/workflows/buildntest.yml)
[![CodeQL](https://github.com/rob-seiler/luacpp/actions/workflows/codeql.yml/badge.svg)](https://github.com/rob-seiler/luacpp/actions/workflows/codeql.yml)

# luacpp

A simple C++17 wrapper for Lua 5.5.

## Why another library?

Well the answer is simple. I just wanted to. For me the best way to learn something is to do it on my own. I had to implement Lua for work and got the permission to publish my wrapper on github :)

## Building

### Prerequisites

- A C++17 compiler
- CMake 3.10 or later
- The `googletest` submodule (optional — only needed if you want to build the unit tests)

### Steps

```bash
git clone https://github.com/rob-seiler/luacpp.git
cd luacpp
git submodule update --init --recursive   # optional, for unit tests
mkdir build && cd build
cmake ..
cmake --build . --target luacpp
cmake --build . --target luacpp_test      # optional
```

### CMake options

| Option | Default | Effect |
|---|---|---|
| `BUILD_EXAMPLES` | `ON` | Build the small example programs under `examples/`. |
| `LUACPP_ENABLE_CMODULE_LOADING` | `OFF` | Allow Lua scripts to load compiled C modules at runtime via `require()`. See [docs/c-modules.md](docs/c-modules.md) for the deployment implications. |

## Quickstart

```c++
#include <luacpp/State.hpp>

int main() {
    const char* src = "x = 10 + 2";
    Lua::State lua;

    lua.loadAndExecuteScript(src);
    const int x = lua.readVariable<int>("x");
    return 0;
}
```

A fresh `Lua::State` opens no standard libraries — scripts run in a sealed sandbox by default. To enable `math`, `string`, etc. see [docs/lua-libraries.md](docs/lua-libraries.md).

## Documentation

- [Embedding C++ functions in Lua](docs/embedding-functions.md) — `registerNativeFunction`, upvalues, `registerMethod` for `std::function` callbacks and lambdas with capture.
- [Reading and writing values](docs/values-and-tables.md) — `readVariable` / `writeVariable`, homogeneous and generic table reads, nested tables via `withTableDo`.
- [Binding C++ classes to Lua](docs/class-binding.md) — `Metatable<T>` auto-detection of operators, `bindConstructor` / `bindMethod` / `bindProperty`, static fields, ownership semantics.
- [Lua standard libraries](docs/lua-libraries.md) — Library bits, eager opening vs `require()`-based preloading, extending `package.path` / `package.cpath`.
- [Loading C modules at runtime](docs/c-modules.md) — `LUACPP_ENABLE_CMODULE_LOADING`, POSIX vs Windows deployment, LuaRocks-compatible C-API contract.
- [External strings and ownership transfer](docs/external-strings.md) — Lua 5.5's `lua_pushexternalstring` wrapper, zero-copy buffer sharing, transferring container lifetime to Lua's GC.

## Examples

Runnable end-to-end examples live in [`examples/`](examples/):

- [`examples/basics/`](examples/basics/) — minimal embedding setup.
- [`examples/gameoflife/`](examples/gameoflife/) — a small simulation driven by a Lua script, demonstrating class binding and script-driven application logic.

## Roadmap

- **Improved error handling** — currently only `loadAndExecuteScript` populates the error list; other execution paths discard Lua's error message. Planned as the headline feature for v0.3.0.
- **Coroutine support.**
- **Custom Lua allocator support** — let the host install a `lua_Alloc` for tracking, pooling, or constraining Lua's memory.
- **Improved debug hooks** — richer abstractions around `lua_sethook`.
- **C++20 modules** via a wrapper module (`luacpp.cppm`) that re-exports the existing headers — gives consumer projects `import luacpp;` without forcing internal module restructuring. Considered a worthwhile selling point for larger downstream codebases.
- **Faster member access on bound classes** — for types with methods only (no properties), set `__index` directly to a methods table so Lua resolves lookups via `rawget` instead of crossing into a C dispatcher (`propertyIndexDispatcher`). Several times cheaper in hot loops; worth measuring before generalising.
- **Reflection-based auto-binding (C++26, P2996)** — a single `shareInLua<T>(state)` call that registers every public data member and member function on `T`'s metatable, with opt-out annotations (`[[=Lua::reflect::skip]]`, `[[=Lua::reflect::rename("...")]]`). Blocked on mainline Clang/GCC/MSVC support for P2996.

## License

See [LICENSE](LICENSE).
