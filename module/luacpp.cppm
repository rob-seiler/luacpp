// C++20 named-module wrapper for luacpp.
//
// All public headers are pulled in via the global module fragment so that
// internal cyclic dependencies (State <-> Table <-> Registry) keep working
// through forward-declares — exactly as in the static-library build. The
// module surface below re-exports only what consumers are meant to reach.
//
// Build only when LUACPP_BUILD_MODULE=ON is passed to CMake (requires a
// CMake >= 3.28 toolchain with C++20 module support). The default static
// library at include/luacpp/*.hpp remains unchanged and authoritative.

module;

#include <luacpp/Basics.hpp>
#include <luacpp/Debug.hpp>
#include <luacpp/Generic.hpp>
#include <luacpp/Metatable.hpp>
#include <luacpp/Registry.hpp>
#include <luacpp/Stack.hpp>
#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Type.hpp>
#include <luacpp/TypeMismatchException.hpp>
#include <luacpp/Version.hpp>

export module luacpp;

export namespace Lua {

// Core state and table abstractions
using ::Lua::State;
using ::Lua::Table;
using ::Lua::Registry;

// Value / type machinery
using ::Lua::Generic;
using ::Lua::Type;
using ::Lua::toString;
using ::Lua::Basics;
using ::Lua::TypeMismatchException;

// Class binding
using ::Lua::Metatable;
using ::Lua::Bind;

// Stack interface
using ::Lua::Stack;
using ::Lua::pushToStack;
using ::Lua::getStackValue;

// Versioning
using ::Lua::Version;
using ::Lua::LuaCppVersion;

// Debug
using ::Lua::Debug;
using ::Lua::DebugInfo;
using ::Lua::EventCodes;
using ::Lua::MaskCall;
using ::Lua::MaskReturn;
using ::Lua::MaskLine;
using ::Lua::MaskCount;

// Small utilities
using ::Lua::bit;

} // namespace Lua
