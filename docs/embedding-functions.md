# Embedding C++ functions in Lua

Loading and executing a script doesn't provide much benefit without exposing your own functions so the Lua side can call back into the application. luacpp offers several mechanisms for this — from a plain C-style callback up to lambdas with capture and bound member functions.

## Plain C-style callbacks: `registerNativeFunction`

For simple functions with no dependencies, the lightest option is `registerNativeFunction`. It takes the same signature as a raw `lua_CFunction`.

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

## Carrying state through upvalues: `registerNativeFunctionWithUpvalues`

If your callback needs access to an object, attach it as an upvalue. Inside the function you read it back via `getUpValue`.

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

## Lambdas and captures: `registerMethod`

For more flexible cases, `registerMethod` accepts a `std::function<int(State&)>`. That lets you pass lambdas with capture lists or member functions bound via `std::bind`, without writing the upvalue plumbing yourself.

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

`setReturnValue` pushes its arguments onto the stack and returns the count — exactly what Lua expects as the return value of a C callback.

## When to use which

| Method | Use when |
|---|---|
| `registerNativeFunction` | Function is stateless or only depends on global state |
| `registerNativeFunctionWithUpvalues` | Function needs a fixed C++ object (passed by pointer) |
| `registerMethod` | Function needs a lambda capture, member binding, or any `std::function` |
