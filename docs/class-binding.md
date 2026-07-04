# Binding C++ classes to Lua

luacpp can expose entire C++ classes to Lua as userdata with their own metatable. The `Metatable<T>` template inspects `T` at compile time and registers a default set of metamethods for whichever operators `T` actually provides — `+`, `-`, `*`, `/`, unary `-`, `==`, `<`, `<=`, a `toString()` method and (if `T` is not trivially destructible) a `__gc` handler that calls the destructor.

## Minimal binding

Class binding lives in its own header — include `<luacpp/Bind.hpp>` (it pulls in `<luacpp/State.hpp>` for you) in any translation unit that calls `state.binding.*`:

```c++
#include <luacpp/Bind.hpp>
#include <luacpp/Metatable.hpp>

struct Vector {
    Vector(float ax = 0, float ay = 0) : x(ax), y(ay) {}
    Vector operator+(const Vector& rhs) const { return Vector(x + rhs.x, y + rhs.y); }
    Vector operator-() const { return Vector(-x, -y); }
    bool operator==(const Vector& rhs) const { return x == rhs.x && y == rhs.y; }
    float x;
    float y;
};

int main() {
    Lua::State lua(Lua::State::LibBase);
    Lua::Metatable<Vector>::registerMetatable(lua);
    lua.binding.constructor<Vector, float, float>("Vector");

    lua.loadAndExecuteScript("v = Vector(1, 2) + Vector(3, 4)");
    auto v = lua.variables.read<Vector*>("v"); // std::optional<Vector*>
    if (v) {
        // (*v)->x == 4, (*v)->y == 6
    }
    return 0;
}
```

`binding.constructor` creates a callable Lua table so `Vector(1, 2)` in Lua constructs the object directly into Lua-owned userdata — no extra copy, no factory function required.

## Member functions and data members

You can also expose member functions and data members:

```c++
struct Vec {
    float x, y;
    Vec(float ax, float ay) : x(ax), y(ay) {}
    float length() const { return std::sqrt(x * x + y * y); }
    Vec scaled(float s) const { return Vec(x * s, y * s); }
};

Lua::Metatable<Vec>::registerMetatable(lua);
lua.binding.constructor<Vec, float, float>("Vec");
lua.binding.method<Vec, &Vec::length>("length");
lua.binding.method<Vec, &Vec::scaled>("scaled");
lua.binding.property<Vec, &Vec::x>("x");
lua.binding.property<Vec, &Vec::y>("y");
```

In Lua you can then do:
```lua
v = Vec(3, 4)
print(v:length())       -- method call via colon syntax
v2 = v:scaled(2)
v.x = 10                -- property write
print(v.x)              -- property read
```

## Static fields and free functions

`binding.staticField` and `binding.staticFunction` attach values or free functions to the constructor table, so calls like `Vec.EPSILON` or `Vec.fromAngle(pi)` work as well:

```c++
constexpr float kEpsilon = 1e-6f;
lua.binding.staticField("Vec", "EPSILON", kEpsilon);

Vec fromAngle(float radians) {
    return Vec(std::cos(radians), std::sin(radians));
}
lua.binding.staticFunction<&fromAngle>("Vec", "fromAngle");
```

```lua
print(Vec.EPSILON)          -- 1e-06
local v = Vec.fromAngle(0)  -- Vec(1, 0)
```

## Metatable names

Every bound class is registered in Lua's registry under a metatable name.
The default (since v0.3.0) is `"luacpp."` plus the qualified C++ type name,
computed at compile time:

```
luacpp.Vec
luacpp.myns::Grid
```

For named, non-template types this string is **identical across MSVC, GCC
and Clang** — the reason this matters is the host/plugin scenario: a host
application and a plugin built with different compilers can share one
`lua_State` and still recognize each other's bound types. (Previously the
default was `typeid(T).name()`, which is compiler-specific; a host and
plugin built against luacpp < 0.3.0 and ≥ 0.3.0 will not match either —
upgrade both sides together.)

The name is visible wherever Lua uses the metatable's `__name` field: the
default `tostring()` output (`luacpp.Vec: 0x0000019f...`) and type-check
error messages (`bad argument #1 (luacpp.Vec expected, got number)`). The
`luacpp.` prefix keeps the registry key from colliding with metatables you
create yourself via `createMetaTable("Vec", ...)`.

Not guaranteed to match across compilers (deterministic per compiler only):
template instantiations (spelling of arguments differs) and types in
anonymous namespaces. For those — or to pick a custom name — specialize
`Metatable<T>`:

```c++
template <>
struct Lua::Metatable<myns::Grid> {
	static const char* metatableName() { return "luacpp.Grid"; }

	static void registerMetatable(Lua::State& state) {
		Lua::detail::registerDefaultMetatable<myns::Grid>(state);
	}

	template <typename... Args>
	static myns::Grid* create(Lua::State& state, Args&&... args) {
		myns::Grid* obj = state.createUserData<myns::Grid>(std::forward<Args>(args)...);
		state.assignMetaTable(metatableName());
		return obj;
	}
};
```

## A note on ownership

When a C++ value is pushed to Lua (e.g. as the return value of a bound method), it is *copied* into a fresh userdata that Lua owns and garbage-collects. Returning `T&` or `T*` from a bound method does not preserve aliasing — mutations on the Lua side will not propagate back to the original C++ object. To expose live state, bind explicit accessor methods rather than returning references or pointers.
