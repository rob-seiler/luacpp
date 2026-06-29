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

#include <array>
#include <span>
#include <string>

import luacpp;

namespace {

int testReadWriteVariable() {
    Lua::State lua(Lua::State::LibMath);
    lua.loadAndExecuteScript("x = 10 + 2");
    auto x = lua.variables.read<int>("x");
    return (x && *x == 12) ? 0 : 1;
}

int testNativeFunctionRoundtrip() {
    Lua::State lua;
    lua.registerMethod("score", [](Lua::State& s) -> int {
        return s.setReturnValue(42);
    });
    lua.loadAndExecuteScript("y = score()");
    auto y = lua.variables.read<int>("y");
    return (y && *y == 42) ? 0 : 1;
}

int testStringRoundtrip() {
    Lua::State lua(Lua::State::LibString);
    lua.variables.write<const char*>("greeting", "hello");
    lua.loadAndExecuteScript("greeting = greeting .. ' world'");
    auto g = lua.variables.read<std::string>("greeting");
    return (g && *g == "hello world") ? 0 : 1;
}

int testSpanArgsArray() {
    // Exercises the C++20-only std::span overload of
    // executeFunctionWithArgsArray purely through the module surface — the
    // gtest suite is built at C++17 (LUACPP_HAS_SPAN=0) and so cannot cover
    // this path. The overload reaches us here because the module interface
    // was compiled at C++20, where it is part of the exported State class.
    Lua::State lua;
    lua.loadAndExecuteScript("function add3(a, b, c) return a + b + c end");
    const std::array<int, 3> args{2, 3, 4};
    auto sum = lua.executeFunctionWithArgsArrayReturning("add3", std::span(args));
    return (sum && *sum == 9) ? 0 : 1;
}

int testFacadeTypeNamable() {
    // Proves the facade types are re-exported by name (not just reachable via
    // member access): a consumer can bind a reference of the exported type.
    Lua::State lua(Lua::State::LibMath);
    Lua::Variables& vars = lua.variables;
    vars.write<int>("z", 7);
    auto z = vars.read<int>("z");
    return (z && *z == 7) ? 0 : 1;
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
    if (testSpanArgsArray() != 0) return 50;
    if (testFacadeTypeNamable() != 0) return 60;
    return 0;
}
