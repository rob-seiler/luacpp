# External strings and ownership transfer

Lua 5.5 introduced `lua_pushexternalstring`, which lets Lua hold a reference to a string buffer the host owns — without copying the bytes. luacpp wraps this as `pushExternalString` and pairs it with `transferOwnership` for the case where you want to hand the buffer's lifetime to Lua's garbage collector.

## When to care

Most strings should just go through `writeVariable` or `pushToStack`, both of which copy. External strings only pay off for:

- **Large buffers** where a copy would be wasteful (log data, file contents, payloads).
- **Long-lived strings** that already live in a stable container on the host side and outlive the Lua state, so referencing them is safe.

If neither applies, prefer the copying APIs — they have no lifetime contract to honor.

## Referencing a host-owned buffer

```c++
#include <luacpp/State.hpp>

Lua::State lua(Lua::State::LibBase);
std::string payload = loadHugePayload();  // many MB

lua.pushExternalString(payload);          // no copy
lua.setGlobalFromStack("payload");

// payload must stay alive and unmodified until lua_close() runs
// (or until ownership is transferred — see below).
```

**Contract you must hold:**

- The buffer behind the string must outlive every Lua reference to it.
- The bytes must not change while Lua holds the reference — Lua relies on string immutability for hash caching and interning.

Pushing a temporary by rvalue is explicitly deleted on the API to prevent accidental dangling references:

```c++
lua.pushExternalString(std::string("oops"));  // does not compile
```

### SBO note

Short strings often live inside the `std::string` object itself (Short String Optimization). For those, `pushExternalString` silently falls back to a copy — externally referencing inline storage would tie Lua to the address of the `std::string` header, which is not safe. The cost of the copy for short strings is negligible.

## Transferring ownership to Lua's GC

If you want to publish a string into Lua and *then* forget about it on the C++ side, `transferOwnership` moves the holding container into a userdata that Lua's `__gc` will clean up:

```c++
std::string payload = loadHugePayload();

lua.pushExternalString(payload);
lua.setGlobalFromStack("payload");
lua.transferOwnership(std::move(payload));   // payload is moved-from
```

After the call, the original `std::string` is moved-from — don't access it. The buffer survives until `lua_close()` or until you explicitly release the registry reference (not currently exposed in the API).

## Transferring containers, not just strings

`transferOwnership` works for any container `T` that is *node-address-stable under move* — that is, moving the container does not relocate its elements. The standard library types this includes are:

- `std::map`, `std::unordered_map`
- `std::list`, `std::deque`
- `std::string` (special-cased: the SBO/heap split from `pushExternalString` is mirrored here)

Vectors and arrays do *not* qualify, because moving them can change element addresses. The typical pattern is: build a `std::map<std::string, std::string>` of payloads on the host side, push individual values via `pushExternalString`, then hand the whole map to `transferOwnership` so all referenced buffers stay anchored together.
