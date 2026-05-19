# Lua standard libraries

A fresh `Lua::State` opens no standard libraries by default — scripts get a sealed sandbox. You opt into libraries either eagerly (open at startup, globals available immediately) or lazily (register for `require()`, loaded on first use).

## Library bits

Each standard library is represented by a bit in `Lua::State::Library`:

| Bit | Library | Notes |
|---|---|---|
| `LibBase` | base | `print`, `pairs`, `ipairs`, `tostring`, ... |
| `LibPackage` | package | `require`, `package.path`, `package.cpath` |
| `LibCoroutine` | coroutine | `coroutine.create`, `resume`, `yield` |
| `LibDebug` | debug | `debug.traceback`, hooks |
| `LibIO` | io | file I/O |
| `LibMath` | math | `math.sqrt`, `math.pi`, ... |
| `LibOS` | os | `os.time`, `os.getenv` |
| `LibString` | string | `string.format`, pattern matching |
| `LibTable` | table | `table.insert`, `table.sort` |
| `LibUTF8` | utf8 | UTF-8 helpers |
| `LibAll` | — | all of the above |
| `LibNone` | — | none (default) |

Combine with bitwise OR: `LibMath | LibString | LibTable`.

## Eager loading: `openLibrary`

Pass a library mask to the constructor, or call `openLibrary` later. Each selected library is opened immediately and its global table becomes available to scripts.

```c++
Lua::State lua(Lua::State::LibMath | Lua::State::LibString);
lua.loadAndExecuteScript("x = math.sqrt(2)");
```

Same effect, called explicitly:

```c++
Lua::State lua;
lua.openLibrary(Lua::State::LibMath | Lua::State::LibString);
```

## Lazy loading: `preloadLibrary`

For libraries you do not need straight away, `preloadLibrary` registers them in `package.preload` so scripts can pull them in on demand via `require()`. Until a `require` call runs, the library's globals are absent — useful for keeping the script-visible environment minimal at startup.

```c++
Lua::State lua(Lua::State::LibPackage);
lua.preloadLibrary(Lua::State::LibMath);

lua.loadAndExecuteScript(R"(
    -- math is not visible here
    local m = require("math")
    -- now it is, but only inside this script via local m
    print(m.sqrt(2))
)");
```

Note: `preloadLibrary` only makes sense when `LibPackage` is already open — otherwise `require()` does not exist on the script side. The two modes can be combined freely: open some libraries eagerly, preload others.

## Extending the module search path: `addModuleSearchPath`

When `LibPackage` is open, you can extend `package.path` (for `.lua` modules) or `package.cpath` (for native shared-library modules) with custom patterns. Patterns use Lua's `?`-substitution: `require("foo")` substitutes `foo` for `?` and tries each pattern in order.

```c++
Lua::State lua(Lua::State::LibBase | Lua::State::LibPackage);
lua.addModuleSearchPath("/opt/myapp/scripts/?.lua");
// require("config") now looks for /opt/myapp/scripts/config.lua
```

The new pattern is *prepended*, so it wins over Lua's built-in defaults. For native modules pass `forNativeModule = true`:

```c++
lua.addModuleSearchPath("/opt/myapp/lib/?.so", /*forNativeModule=*/true);
```

For loading C modules via `require()` see [Loading C modules at runtime](c-modules.md).
