// Module-surface smoke test.
//
// This translation unit reaches the library exclusively through
//     import luacpp;
// and does NOT include any <luacpp/*.hpp> header. A successful build proves
// the `.cppm` re-exports the public API; a successful run proves the symbols
// resolve correctly across the module boundary at link time.
//
// Wired into CTest only when LUACPP_BUILD_MODULE=ON. Returns a non-zero exit
// code on the first failed expectation so CTest reports which check broke.

import luacpp;

#include <string>

namespace {

int testReadWriteVariable() {
    Lua::State lua(Lua::State::LibMath);
    if (lua.loadAndExecuteScript("x = 10 + 2") != 0) return 1;
    return lua.readVariable<int>("x") == 12 ? 0 : 1;
}

int testNativeFunctionRoundtrip() {
    Lua::State lua;
    lua.registerMethod("score", [](Lua::State& s) -> int {
        return s.setReturnValue(42);
    });
    if (lua.loadAndExecuteScript("y = score()") != 0) return 1;
    return lua.readVariable<int>("y") == 42 ? 0 : 1;
}

int testStringRoundtrip() {
    Lua::State lua(Lua::State::LibString);
    lua.writeVariable<const char*>("greeting", "hello");
    if (lua.loadAndExecuteScript("greeting = greeting .. ' world'") != 0) return 1;
    return lua.readVariable<std::string>("greeting") == "hello world" ? 0 : 1;
}

int testVersionConstant() {
    // Just prove the constant is reachable through the module surface and
    // carries a non-degenerate value — hardcoding the current version here
    // would force every release bump to touch this smoke test.
    constexpr auto v = Lua::LuaCppVersion;
    return v.toNumber() != 0 ? 0 : 1;
}

}  // namespace

int main() {
    if (testReadWriteVariable() != 0) return 10;
    if (testNativeFunctionRoundtrip() != 0) return 20;
    if (testStringRoundtrip() != 0) return 30;
    if (testVersionConstant() != 0) return 40;
    return 0;
}
