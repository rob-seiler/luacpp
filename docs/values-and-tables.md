# Reading and writing values

Another way of interacting between Lua and C++ is by reading and writing variables directly. luacpp provides type-templated accessors for primitives and several flavors for tables.

## Primitive types

To pass variables from C++ to Lua, use `writeVariable`:

```c++
Lua::State state;
state.writeVariable<double>("x", 3.1415);
```

And to read them back, `readVariable`:

```c++
Lua::State state;
double x = state.readVariable<double>("x");
```

For interacting with function arguments and upvalues inside a callback there are dedicated helpers — `getArgument<T>(index)` and `getUpValue<T>(index)`.

## Tables

Tables come in several flavors depending on how much you know about their contents at compile time.

### Homogeneous tables: `readTable` / `readTableIfMatching`

The simplest case is a table where every value has the same type. `readTable` throws `Lua::TypeMismatchException` on a type mismatch; `readTableIfMatching` silently skips offending entries.

```c++
Lua::State state;
try {
    std::map<std::string, double> values = state.readTable<std::string, double>("lut");
} catch (const Lua::TypeMismatchException& e) {
    // handle exception
}
```

### Type-erased reads: `readTableGeneric`

If you want to inspect a table without committing to a value type, `readTableGeneric` wraps each cell in a `Generic` that can be queried or converted to string.

```c++
Lua::State state;
auto map = state.readTableGeneric("table");
for (const auto& [key, value] : map) {
    std::cout << key.toString() << ": " << value.toString() << std::endl;
}
```

### Nested tables: `withTableDo`

For tables with mixed value types or nested structures, `withTableDo` hands you a `Table` object scoped to a callback. The table is automatically popped off the stack when the callback returns.

```c++
int a;
float b;
std::string c;

Lua::State state;
state.withTableDo("map", [&a, &b, &c](Lua::Table& table) {
    table.readValue<int>("a", a);
    table.readValue<float>("b", b);
    table.readValue<std::string>("c", c);
}, false);  // false = do not create if missing
```

The same method also writes — read and write are both available on the `Table` object.

### Trivial writes: `writeTable`

For simple homogeneous tables there is also a one-liner:

```c++
std::map<std::string, int> table = { { "a", 10 }, { "b", 20 } };
Lua::State state;
state.writeTable("map", table);
```
