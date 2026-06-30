# Reading and writing values

Another way of interacting between Lua and C++ is by reading and writing variables directly. luacpp provides type-templated accessors for primitives and several flavors for tables.

## Primitive types

To pass variables from C++ to Lua, use `variables.write`:

```c++
Lua::State state;
state.variables.write<double>("x", 3.1415);
```

And to read them back, `variables.read`:

```c++
Lua::State state;
auto x = state.variables.read<double>("x"); // std::optional<double>
if (x) {
    // *x is the value
}
```

`variables.read` returns `std::optional<T>` so that "global not set" and "global has a different type" both surface as `nullopt` — querying for a value is not the same as treating its absence as an error.

For interacting with function arguments and upvalues inside a callback there are dedicated helpers — `getArgument<T>(index)` and `getUpValue<T>(index)`.

## Tables

Tables come in several flavors depending on how much you know about their contents at compile time.

### Homogeneous tables: `variables.readTable` / `variables.readTableIfMatching`

The simplest case is a table where every value has the same type. `variables.readTable` throws `Lua::TypeMismatchException` on a type mismatch; `variables.readTableIfMatching` silently skips offending entries.

```c++
Lua::State state;
try {
    std::map<std::string, double> values = state.variables.readTable<std::string, double>("lut");
} catch (const Lua::TypeMismatchException& e) {
    // handle exception
}
```

### Type-erased reads: `variables.readTableGeneric`

If you want to inspect a table without committing to a value type, `variables.readTableGeneric` wraps each cell in a `Generic` that can be queried or converted to string.

```c++
Lua::State state;
auto map = state.variables.readTableGeneric("table");
for (const auto& [key, value] : map) {
    std::cout << key.toString() << ": " << value.toString() << std::endl;
}
```

### Nested tables: `variables.withTableDo`

For tables with mixed value types or nested structures, `variables.withTableDo` hands you a `Table` object scoped to a callback. The table is automatically popped off the stack when the callback returns.

```c++
int a;
float b;
std::string c;

Lua::State state;
state.variables.withTableDo("map", [&a, &b, &c](Lua::Table& table) {
    table.readValue<int>("a", a);
    table.readValue<float>("b", b);
    table.readValue<std::string>("c", c);
}, false);  // false = do not create if missing
```

The same method also writes — read and write are both available on the `Table` object.

### Trivial writes: `variables.writeTable`

For simple homogeneous tables there is also a one-liner:

```c++
std::map<std::string, int> table = { { "a", 10 }, { "b", 20 } };
Lua::State state;
state.variables.writeTable("map", table);
```
