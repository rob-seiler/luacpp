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
    auto x = lua.variables.read<int>("x"); // std::optional<int>
    return (x && *x == 12) ? 0 : 1;
}
```

To load from a file, pass a `Lua::File` (alias for `std::filesystem::path`) — Lua tracebacks then reference the actual file path:

```c++
lua.loadAndExecuteScript(Lua::File("scripts/main.lua"));
```

A fresh `Lua::State` opens no standard libraries — scripts run in a sealed sandbox by default. To enable `math`, `string`, etc. see [docs/lua-libraries.md](docs/lua-libraries.md).

## Documentation

- [Embedding C++ functions in Lua](docs/embedding-functions.md) — `registerNativeFunction`, upvalues, `registerMethod` for `std::function` callbacks and lambdas with capture.
- [Reading and writing values](docs/values-and-tables.md) — `variables.read` / `variables.write`, homogeneous and generic table reads, nested tables via `variables.withTableDo`.
- [Binding C++ classes to Lua](docs/class-binding.md) — `Metatable<T>` auto-detection of operators, `binding.constructor` / `binding.method` / `binding.property`, static fields, ownership semantics.
- [Lua standard libraries](docs/lua-libraries.md) — Library bits, eager opening vs `require()`-based preloading, extending `package.path` / `package.cpath`.
- [Loading C modules at runtime](docs/c-modules.md) — `LUACPP_ENABLE_CMODULE_LOADING`, POSIX vs Windows deployment, LuaRocks-compatible C-API contract.
- [External strings and ownership transfer](docs/external-strings.md) — Lua 5.5's `lua_pushexternalstring` wrapper, zero-copy buffer sharing, transferring container lifetime to Lua's GC.
- [C++20 module wrapper](docs/cpp20-modules.md) — opt-in `import luacpp;` surface for consumers that already build with modules.

## Examples

Runnable end-to-end examples live in [`examples/`](examples/):

- [`examples/basics/`](examples/basics/) — minimal embedding setup.
- [`examples/gameoflife/`](examples/gameoflife/) — a small simulation driven by a Lua script, demonstrating class binding and script-driven application logic.

## Roadmap

### v0.3.0 — Embedding hardening

- **Lua traceback support** via opt-in `pcall` message handler.
- **Stable metatable names** — replace the `typeid(T).name()` default.

### v0.4.0 — Class binding expansion

- **Inheritance / base classes** for bound types.
- **Smart pointer ownership** — `unique_ptr<T>` / `shared_ptr<T>` / `weak_ptr<T>`.
- **Read-only and computed properties.**
- **Enum binding.**
- **`std::optional`, multi-return, and tuple support** in `Stack<T>` / `pushResult`.
- **Integer / double subtype distinction** in `Stack<T>` and `Generic`.
- **Bitwise operator auto-detection** in `Metatable<T>` — `__band`/`__bor`/`__bxor`/`__bnot`/`__shl`/`__shr`.

### v0.5.0 — Coroutines

- **Coroutine creation / resume / yield** from C++.
- **Yieldable native functions** via `lua_yieldk` continuations.

### v0.6.0 — Lua values from C++

- **`LuaRef`** — persistent strong references to Lua tables / functions / userdata.
- **Function overload resolution** by Lua argument types.

### Later / unscheduled

- **Custom Lua allocator support.**
- **Improved debug hooks.**
- **Faster member access on bound classes** — methods-only fast path via `__index` rawget.
- **Reflection-based auto-binding** (C++26, P2996).
- **Sequence container binding** — `std::vector` / `std::array` as array-style tables.
- **`std::variant` and `std::any` conversion.**
- **`__close` support** for Lua 5.4+ to-be-closed variables.
- **Sandbox helpers** — instruction-count / memory limits.
- **Lua value pretty-printing and serialization.**
- **Hot-reload helpers.**
- **`luaL_Buffer` wrapper** for efficient string building from C++.
- **Custom `package.searcher` registration** — asset-pack / network module loaders.
- **Weak table helpers** — `__mode = "k"/"v"/"kv"` convenience.
- **Coroutine recycling** via `lua_resetthread` (Lua 5.4+).

## License

See [LICENSE](LICENSE).
