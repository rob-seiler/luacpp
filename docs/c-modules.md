# Loading C modules at runtime

luacpp can host Lua scripts that pull in compiled C modules (the kind LuaRocks installs) via `require()`. This is off by default because it changes the deployment model — instead of a single statically linked binary you now have a Lua runtime that other modules can link against.

## Enabling the feature

Build with the CMake option `LUACPP_ENABLE_CMODULE_LOADING=ON`:

```bash
cmake -DLUACPP_ENABLE_CMODULE_LOADING=ON ..
```

**What changes when this is on:**

- **POSIX:** Lua stays statically linked. The host executable gets `ENABLE_EXPORTS` (which is `-rdynamic` on Linux) so that `dlopen`'d modules resolve `lua_*` symbols against the host's exported symbol table at load time.
- **Windows:** Lua is built as a SHARED library (`lua.dll`). The host EXE and every C module link against the import library; the DLL must be deployed alongside the application. Windows has no process-wide symbol table equivalent to POSIX's `dlopen`, so a single shared Lua image is the only clean way to share state across host and modules.

## Loading a module from a script

Once enabled, the workflow is just Lua's normal `require()`. You need at least `LibBase` and `LibPackage` open, plus a search path pointing at wherever the module's shared library lives:

```c++
Lua::State lua(Lua::State::LibBase | Lua::State::LibPackage);

#ifdef _WIN32
lua.addModuleSearchPath("./plugins/?.dll", /*forNativeModule=*/true);
#else
lua.addModuleSearchPath("./plugins/?.so", /*forNativeModule=*/true);
#endif

lua.loadAndExecuteScript(R"(
    local uuid = require("uuid")
    id = uuid.v4()
)");
```

The native module itself is a regular Lua C extension: it exports `luaopen_<modulename>` and is linked against the same Lua headers as luacpp uses. See [`plugins/uuid/`](../plugins/uuid/) for a bundled example that doubles as the `require()`-based smoke test in the test suite.

## C API and `extern "C"` compatibility

luacpp builds Lua with its public API declared `extern "C"` so that C-language modules — LuaRocks packages, hand-written extensions, anything compiled with a C compiler — can link against the resulting library without name-mangling mismatches. A small C-only smoke test (`luacpp_c_linkage_check`) lives in the test suite as a regression guard for this contract.

## When *not* to enable this

If your application embeds Lua only to run scripts you control, leave this off. The static single-binary deployment is simpler: no shared library to ship, no search path to manage, no symbol-resolution edge cases. Enable C-module loading only when scripts need to pull in third-party native code at runtime.
