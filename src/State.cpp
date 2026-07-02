#include <State.hpp>
#include <lua/lua.hpp>

#include <cassert>
#include <stdexcept> //std::logic_error
#include <string>
#include <limits>    //std::numeric_limits

namespace {
// Detects whether s.data() points into the std::string object itself (Short
// String Optimization). Portable across libstdc++, libc++ and MSVC. uintptr_t
// conversion sidesteps the UB of comparing pointers from different objects.
bool hasInlineStorage(const std::string& s) {
	const auto data  = reinterpret_cast<std::uintptr_t>(s.data());
	const auto begin = reinterpret_cast<std::uintptr_t>(&s);
	return data >= begin && data < begin + sizeof(std::string);
}

// Statuses for which Lua left an error object on the top of the stack.
// Everything else (Ok, Yield, and the synthetic luacpp-side codes with
// negative values) carries its message inline and must NOT trigger a
// stack pop — luaL_tolstring on whatever happens to be at -1 would
// stringify a bogus value (or push nil on an empty stack).
bool hasStackError(Lua::LuaError::Status s) {
	using S = Lua::LuaError::Status;
	switch (s) {
		case S::RuntimeError:
		case S::SyntaxError:
		case S::MemoryError:
		case S::MsgHandlerError:
		case S::FileError:
			return true;
		default:
			return false;
	}
}

// pcall message handler, modelled on lua.c's msghandler. One deviation:
// __tostring error objects get a traceback appended too (lua.c returns
// them bare) — a traceback is the whole point of opting in.
int tracebackHandler(lua_State* L) {
	const char* msg = lua_tostring(L, 1);
	if (msg == nullptr) { // non-string error object, e.g. error({...})
		if (luaL_callmeta(L, 1, "__tostring") &&
		    lua_type(L, -1) == LUA_TSTRING) {
			msg = lua_tostring(L, -1); // anchored below the traceback buffer
		} else {
			msg = lua_pushfstring(L, "(error object is a %s value)",
			                      luaL_typename(L, 1));
		}
	}
	luaL_traceback(L, L, msg, 1);
	return 1;
}
} // namespace

namespace Lua {

State::State(Library libraries)
: m_state(detail::StateRegistry::newVM()),
  m_context(detail::StateRegistry::acquire(m_state, m_isMain)),
  m_registry(m_state)
{
	// Manual release on body-throw: ~State doesn't run when a ctor body
	// throws and m_context has no RAII handle.
	try {
		openLibrary(libraries);
	} catch (...) {
		detail::StateRegistry::release(m_context);
		throw;
	}
}

State::State(lua_State* state)
: m_state(state),
  m_context(detail::StateRegistry::acquire(m_state, m_isMain)),
  m_registry(m_state)
{
	// No body try/catch: m_registry wraps LUA_REGISTRYINDEX, always a table,
	// so its ctor can't throw on a sane lua_State.
}

State::State(detail::StateContext* ctx, lua_State* state)
: m_state(state),
  m_context(detail::StateRegistry::retain(ctx)),
  m_registry(m_state)
{
	// m_isMain stays default-false — this overload is the borrowed-view
	// path used by Lua trampolines.
}

LuaError State::popErrorFromStack(LuaError::Category category, LuaError::Status status) {
	LuaError err{category, status, {}};

	// Lua guarantees exactly one error object on the stack top after a failed
	// luaL_loadstring / lua_pcall — we must consume it on every path, even
	// when it's not a string (e.g. `error({...})` propagates a table).
	// luaL_tolstring honors __tostring on tables/userdata, so custom error
	// objects still surface a useful message.
	//
	// Plain StackGuard's top-restore semantics keep the stack balanced
	// through every failure mode of luaL_tolstring — including the corner
	// case where a buggy __tostring leaves an extra value behind before
	// erroring. We use the lean variant here (not AssertionStackGuard)
	// because the stringification luaL_tolstring pushes on success is an
	// intentional intermediate value.
	StackGuard guard(m_state);
	size_t len = 0;
	const char* s = luaL_tolstring(m_state, -1, &len);
	err.message = std::string(s, len);
	return err;
}

void State::reportError(LuaError err) {
	if (m_context->policy.logger)  m_context->policy.logger->log(err);
	if (m_context->policy.handler) (*m_context->policy.handler)(err);
}

LuaError::Status State::reportStatus(LuaError::Category category, int rawStatus) {
	return reportStatus(category, static_cast<LuaError::Status>(rawStatus));
}

LuaError::Status State::reportStatus(LuaError::Category category, LuaError::Status status) {
	if (hasStackError(status)) {
		reportError(popErrorFromStack(category, status));
	} else {
		// Ok / Yield / synthetic (negative) statuses carry no stack object.
		// Callers with a synthetic failure must build the LuaError themselves
		// and route it through reportError() — not through this helper.
		assert((status == LuaError::Status::Ok ||
		        status == LuaError::Status::Yield) &&
		       "reportStatus: synthetic status routed through stack-popping path");
	}
	return status;
}

void State::requireOwnedState(const char* api) const {
	if (m_isMain) return;
	throw std::logic_error(
		std::string(api) +
		" requires the main State for this lua_State — the wrapper that "
		"first registered the context. Subsequent wrappers around the same "
		"VM share its state but cannot register per-instance callbacks.");
}

State::~State() {
	// Tear down any debug hook with the main wrapper that registered it —
	// the lambda may capture references whose lifetime is tied to its scope.
	if (m_isMain) {
		m_context->debugHook = {};
		lua_sethook(m_state, nullptr, 0, 0);
	}
	detail::StateRegistry::release(m_context);
}

void State::pushExternalString(const std::string& s) {
	if (hasInlineStorage(s)) {
		// SBO: bytes live inside the std::string object. Referencing them
		// externally would tie Lua to the object's address — not safe. Have
		// Lua make its own copy; the cost is tiny (a handful of bytes).
		Basics::pushString(m_state, s.data(), s.size());
	} else {
		// Heap buffer: address is stable regardless of where the std::string
		// header lives. Safe to reference without copying.
		Basics::pushExternalString(m_state, s.data(), s.size(), nullptr, nullptr);
	}
}

void State::transferStringOwnership(std::string s) {
	// Pre-condition: caller previously pushed this string via pushExternalString.
	// Whether that push went the SBO-copy path or the external-reference path is
	// re-derived here from the same SBO check, so the two stay in sync.
	if (hasInlineStorage(s)) {
		// Lua already owns its own copy from the push. Nothing to anchor.
		return;
	}
	// Long string: swap the heap buffer into a heap-allocated std::string holder.
	// std::string swap on two long strings is a pointer swap — the buffer Lua
	// references does not move. Then hand the holder to anchorOwned, which
	// takes responsibility for either anchoring it in the registry or freeing
	// it if Lua throws partway through.
	auto* holder = new std::string;
	holder->swap(s);
	anchorOwned(holder, &State::deleteTyped<std::string>);
}

void State::anchorOwned(void* ptr, void (*deleter)(void*)) {
	// Local scope guard: owns ptr until ownership is committed to Lua's GC.
	// If any Lua call below throws before the commit point, the guard's
	// destructor runs and frees ptr. After the commit point (lua_setmetatable
	// installs __gc), we release the guard so Lua's GC is the sole owner.
	struct Guard {
		void* ptr;
		void (*deleter)(void*);
		~Guard() { if (ptr) deleter(ptr); }
	} guard{ptr, deleter};

	void** slot = static_cast<void**>(lua_newuserdatauv(m_state, sizeof(void*), 0));
	*slot = ptr;

	lua_createtable(m_state, 0, 1);
	// The __gc closure carries the deleter as its single upvalue (lightuserdata
	// holding a void(*)(void*)). Push order and the upvalueindex(1) read below
	// must stay in sync — if you add upvalues here, fix the index in the lambda.
	lua_pushlightuserdata(m_state, reinterpret_cast<void*>(deleter));
	lua_pushcclosure(m_state, [](lua_State* L) -> int {
		void** s = static_cast<void**>(lua_touserdata(L, 1));
		auto del = reinterpret_cast<void(*)(void*)>(
			lua_touserdata(L, lua_upvalueindex(1)));
		del(*s);
		return 0;
	}, 1);
	lua_setfield(m_state, -2, "__gc");
	lua_setmetatable(m_state, -2);
	// Commit: Lua's GC now owns ptr via __gc. Even if luaL_ref throws below,
	// the userdata is unreferenced after stack unwinding and Lua will collect
	// it, running our deleter exactly once. Releasing the guard here ensures
	// we don't double-free.
	guard.ptr = nullptr;

	luaL_ref(m_state, LUA_REGISTRYINDEX);
}

void State::openLibrary(Library library) {
	luaL_openselectedlibs(m_state, static_cast<int>(library), 0);
}

void State::preloadLibrary(Library library) {
	luaL_openselectedlibs(m_state, 0, static_cast<int>(library));
}

void State::addModuleSearchPath(const std::string& pattern, bool forNativeModule) {
	if (lua_getglobal(m_state, "package") != LUA_TTABLE) {
		lua_pop(m_state, 1);
		return; // LibPackage not loaded — no package table to extend
	}
	DefaultStackGuard packageGuard(m_state); // pops the package table on every exit path

	const char* fieldName = forNativeModule ? "cpath" : "path";

	std::string combined;
	{
		lua_getfield(m_state, -1, fieldName); // push current path/cpath
		DefaultStackGuard pathGuard(m_state); // pops it even if the string ops below throw bad_alloc
		size_t currentLen = 0;
		const char* current = lua_tolstring(m_state, -1, &currentLen);

		combined.reserve(pattern.size() + 1 + currentLen);
		combined.append(pattern);
		if (current && currentLen > 0) {
			combined.push_back(';');
			combined.append(current, currentLen);
		}
	} // pathGuard pops the old path here; `current` is no longer referenced

	lua_pushlstring(m_state, combined.data(), combined.size());
	lua_setfield(m_state, -2, fieldName);
}

void State::registerNativeFunction(const char* name, NativeFunction func, int numUpValues) {
	lua_pushcclosure(m_state, func, numUpValues);
	lua_setglobal(m_state, name);
}

void State::registerMethod(const char* name, Method method) {
	// Owner-only: dispatchMethod captures `this`, m_callbacks is per-State.
	requireOwnedState("registerMethod");
	m_callbacks.push_back(method);
	registerNativeFunctionWithUpvalues(name, dispatchMethod, m_callbacks.size() - 1, this);
}

void State::installDebugHook(DebugHook hook, int mask, int count) {
	// Owner-only so the hook's tied to a clear lifetime — ~main tears it
	// down before any captured references can dangle.
	requireOwnedState("diagnostics.registerDebugHook");
	m_context->debugHook = std::move(hook);

	auto chook = [](lua_State* L, lua_Debug* ar) {
		// Single lookup; the borrowed-view ctor retains the resolved ctx.
		auto* ctx = detail::StateRegistry::find(L);
		if (ctx && ctx->debugHook) {
			State state(ctx, L);
			ctx->debugHook(state, reinterpret_cast<const DebugInfo&>(*ar));
		}
	};

	lua_sethook(m_state, chook, mask, count);
}

void State::overrideLuaFunction(const char* name, NativeFunction func) {
	lua_getglobal(m_state, GlobalScope); //load global scope to stack
	DefaultStackGuard guard(m_state);    //pops _G on every exit path
	lua_pushcclosure(m_state, func, 0);  //push function to stack
	lua_setfield(m_state, -2, name);     //register under the given name (consumes closure)
}

LuaError::Status State::loadAndExecuteScript(const char* code) {
	// Split load/exec explicitly (instead of luaL_dostring) so that load
	// failures and runtime failures land in distinct LuaError categories.
	const auto loadRc = reportStatus(LuaError::Category::Load, luaL_loadstring(m_state, code));
	if (loadRc != LuaError::Status::Ok) return loadRc;
	return reportStatus(LuaError::Category::Runtime, callFunction(0, LUA_MULTRET));
}

LuaError::Status State::loadAndExecuteScript(const File& path) {
	// Share Registry::loadFile so path-encoding handling (notably non-ASCII
	// paths on Windows) lives in exactly one place. We do NOT use luaL_dofile
	// because that macro expands to (load || pcall), collapsing every non-zero
	// status to 1 — preserving LUA_ERRFILE / LUA_ERRSYNTAX / LUA_ERRRUN is the
	// entire point of the file-loading overload.
	const auto loadRc = reportStatus(LuaError::Category::Load, Registry::loadFile(m_state, path));
	if (loadRc != LuaError::Status::Ok) return loadRc;
	return reportStatus(LuaError::Category::Runtime, callFunction(0, LUA_MULTRET));
}

Type State::getType(int index) const {
	return static_cast<Type>(lua_type(m_state, index));
}

Type State::pushGlobalToStack(const char* name) {
	return Basics::pushGlobal(m_state, name);
}

void State::setGlobalFromStack(const char* name) {
	Basics::setGlobal(m_state, name);
}

int State::getStackSize() const {
	return lua_gettop(m_state);
}

void State::createTable(const char* name, TableFunction workOnTable) {
	lua_newtable(m_state);
	DefaultStackGuard guard(m_state);  // pops the partial table if workOnTable throws
	Table table(m_state, -1);   // the table is on top of the stack
	workOnTable(table);
	guard.release();            // succeeded — commit
	if (name != nullptr) {
		lua_setglobal(m_state, name); // consumes the table
	}
	// else: leave the table on the stack as the caller's return value
}

void State::createMetaTable(const char* name, TableFunction workOnTable) {
	luaL_newmetatable(m_state, name);
	DefaultStackGuard guard(m_state);  // always pops the metatable; also on throw
	Table table(m_state, -1, true); // the table is on top of the stack
	workOnTable(table);
}

bool State::assignMetaTable(const char* name) {
	const bool found = (luaL_getmetatable(m_state, name) == LUA_TTABLE);
	DefaultStackGuard guard(m_state); // governs the value luaL_getmetatable pushed
	if (found) {
		//stack assumption:
		//-1: metatable
		//-2: userdata to assign the metatable to
		lua_setmetatable(m_state, -2); // consumes the metatable
		guard.release();
		return true;
	}
	return false; // guard pops the nil — fixes a previous leak on this path
}

int State::dispatchMethod(lua_State* state) {
	const int32_t index = static_cast<int32_t>(lua_tointeger(state, lua_upvalueindex(1)));
	State* luaState = static_cast<State*>(lua_touserdata(state, lua_upvalueindex(2)));
	return luaState->m_callbacks[index](*luaState);
}

bool State::loadFunction(const char* funcName) {
	if (lua_getglobal(m_state, funcName) == LUA_TFUNCTION) {
		return true; // function left on the stack for the caller to invoke
	}
	lua_pop(m_state, 1); // not a function — don't leak the pushed value
	return false;
}

int State::callFunction(int numArgs, int numResults) {
	// lua_checkstack, not luaL_checkstack: the luaL variant raises, and we
	// are outside any protected frame here — degrade to a plain pcall
	// instead of risking a panic over the handler's one extra slot.
	if (!m_context->tracebackEnabled || !lua_checkstack(m_state, 1)) {
		return lua_pcall(m_state, numArgs, numResults, 0);
	}
	const int base = lua_gettop(m_state) - numArgs; // the function's slot
	lua_pushcfunction(m_state, tracebackHandler);
	lua_insert(m_state, base);
	const int rc = lua_pcall(m_state, numArgs, numResults, base);
	lua_remove(m_state, base); // results shift down; an error object stays on top
	return rc;
}



} // namespace Lua