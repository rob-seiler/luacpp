# Error handling and tracebacks

Every failure luacpp encounters — load errors, runtime errors, luacpp-side
detections like a missing function — is surfaced as a `Lua::LuaError` and
routed through two per-VM slots: a passive **logger** and an active
**handler**. Optionally, runtime errors can carry a full Lua stack
**traceback**.

## LuaError

```c++
struct LuaError {
    Category   category; // Load or Runtime
    Status     status;   // mirrors Lua's codes, plus luacpp-side detections
    LuaMessage message;  // Lua's error string, with parsing helpers
};
```

`Category::Load` means the chunk never ran (`loadstring`/file/syntax/IO
errors); `Category::Runtime` means a protected call failed. Positive
`Status` values mirror Lua's own codes (`RuntimeError`, `SyntaxError`,
`MemoryError`, `MsgHandlerError`, `FileError`); negative values are
detections made by luacpp itself (`FunctionNotFound`,
`RegistryKeyNotFound`, `InvalidKey`) — `isLuacppError()` tells the two
apart. `describe(status)` returns a stable name for logging.

## Logger and handler slots

Both slots live in the per-VM context, so every `State` wrapper around the
same `lua_State` sees the same configuration. The logger observes, the
handler reacts; on an error the logger always fires first.

```c++
Lua::State lua;

// Passive observation. Default is a StreamLogger to std::cerr;
// nullptr silences logging entirely.
auto& mem = lua.diagnostics.installLogger<Lua::MemoryLogger>();

// Active reaction. Default is none (error-code style).
lua.diagnostics.installErrorHandler<Lua::ThrowHandler>();
```

Available loggers: `StreamLogger` (any `std::ostream`), `MemoryLogger`
(accumulates `LuaError`s, pull via `entries()`), `CallbackLogger`
(bridge to spdlog/glog/your own sink). Available handlers: `ThrowHandler`
(throws `LuaException`, which carries the full `LuaError`) and
`CallbackHandler` (arbitrary `std::function` — conditional throw, abort,
custom exception types).

Without a handler, the execute functions simply return their
`LuaError::Status` (or `std::nullopt` for the `*Returning` variants) and
the logger keeps the details.

## LuaMessage

Lua error strings follow the format `<chunkname>:<line>: <text>`.
`LuaMessage` keeps the original string and parses that prefix on demand,
best-effort:

```c++
const Lua::LuaMessage& msg = err.message;
msg.raw();    // the full original string
msg.source(); // std::optional<std::string> — "scripts/main.lua"
msg.line();   // std::optional<int>         — 12
msg.text();   // "attempt to index a nil value (field 'position')"
```

`source()`/`line()` return `nullopt` when the message has no parseable
prefix (e.g. `error({...})` with a table, or `error(msg, 0)`); `text()`
then falls back to the raw string.

## Traceback support (opt-in)

By default a failed call reports only Lua's error message. For embedding
scenarios that want to show *where* a script failed — a script editor, a
game-engine console — enable traceback support:

```c++
lua.diagnostics.setTracebackEnabled(true);
```

From then on, every protected call luacpp makes (`loadAndExecuteScript`,
`executeScript`, `executeFunction` and friends) runs a `luaL_traceback`
message handler, and `LuaError::message` carries the call stack **alongside**
the error text — captured out-of-band, not glued into the message string:

```c++
msg.raw();       // "[string "..."]:3: attempt to index a nil value ..."
msg.text();      // error text only — no prefix
msg.traceback(); // std::optional<Lua::Traceback> — the stack block:
                 //   stack traceback:
                 //           [string "..."]:3: in global 'update_player'
                 //           [string "..."]:7: in global 'on_frame'
                 //           [string "..."]:10: in main chunk
msg.full();      // message + traceback in one printable string
                 // (also LuaException::what() and the StreamLogger output)
```

Because the stack travels separately, `raw()`/`text()`/`source()`/`line()`
behave exactly as with the opt-in disabled, and error text that happens to
contain the phrase `stack traceback:` cannot be mistaken for one.

`Traceback` exposes the block verbatim via `text()`, or parsed via
`asList()` — one `Frame` per stack level, ready for a clickable stack
panel:

```c++
if (auto tb = err.message.traceback()) {
    for (const auto& frame : tb->asList()) {
        // frame.raw    "scripts/main.lua:3: in global 'update_player'"
        // frame.source "scripts/main.lua"   ("[C]" for C frames)
        // frame.line   std::optional<int>   (nullopt for C frames)
        // frame.what   "global 'update_player'", "main chunk", ...
        stackPanel.addRow(frame.raw, frame.source, frame.line);
    }
}
```

Parsing is best-effort in the same spirit as the prefix accessors: lines
that don't match a known frame shape (Lua's `(...tail calls...)` or
`(skipping N levels)`) are kept as frames with only `raw` filled, so no
line is ever lost. Load scripts from a `Lua::File` and the frames
reference the real file path — jump-to-line works out of the box.

See [`examples/traceback/`](../examples/traceback/) for a complete
"editor console" example.

### Details worth knowing

- The flag is per-VM and shared by all wrappers, like the logger/handler
  slots. Default off; there is no cost when disabled.
- **The opt-in is purely additive.** The error object itself is passed
  through untouched and stringifies exactly as in the disabled path
  (length-aware, `__tostring`-honoring) — a plain table stays
  `table: 0x...`, custom `__tostring` messages stay verbatim, embedded
  NULs survive. Only `traceback()` gains a value.
- **Load errors never carry a traceback** — the chunk never ran, so there
  is no call stack. The message stays `file.lua:12: unexpected symbol...`
  and `traceback()` returns `nullopt`.
- **Memory errors** (`Status::MemoryError`): Lua does not invoke message
  handlers on `LUA_ERRMEM`, so those stay bare by design; the same holds
  in the (OOM-adjacent) corner where the Lua stack cannot grow by the
  handler's slots — the call degrades to a plain `pcall` instead of
  panicking.
- An error raised *inside* the message handler surfaces as
  `Status::MsgHandlerError` (`LUA_ERRERR`), reported through the same
  logger/handler pipeline.
