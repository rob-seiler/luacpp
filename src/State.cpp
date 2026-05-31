#include <State.hpp>
#include <lua/lua.hpp>

#include <cassert>
#include <string>
#include <limits> //std::numeric_limits

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
} // namespace

namespace Lua {

std::map<lua_State*, State::DebugHook> State::s_debugHooks;

State::State(Library libraries)
: m_state(luaL_newstate()),
  m_registry(m_state),
  m_externalState(false),
  m_errorLogger(std::make_unique<StreamLogger>()),
  m_errorHandler(nullptr)
{
	// Default warning sink mirrors the error path: loud-to-cerr by default,
	// users override via setWarningLogger / installWarningLogger. Without
	// this, Lua 5.5's own warnfon would print to stderr in a different
	// format, bypassing the [lua warning] tag the library promises.
	setWarningLogger(std::make_unique<StreamWarningLogger>());
	openLibrary(libraries);
}

State::State(lua_State* state)
: m_state(state),
  m_registry(state),
  m_externalState(true),
  m_errorLogger(std::make_unique<StreamLogger>()),
  m_errorHandler(nullptr)
{
	setWarningLogger(std::make_unique<StreamWarningLogger>());
}

void State::setLogger(std::unique_ptr<ErrorLogger> logger) {
	m_errorLogger = std::move(logger);
}

void State::setErrorHandler(std::unique_ptr<ErrorHandler> handler) {
	m_errorHandler = std::move(handler);
}

void State::setWarningLogger(std::unique_ptr<WarningLogger> logger) {
	m_warningLogger = std::move(logger);
	m_warningBuffer.clear();
	if (m_warningLogger) {
		// Installing a logger is explicit opt-in: enable warnings even though
		// Lua starts the system disabled. Scripts can still flip via @off.
		m_warningsEnabled = true;
		lua_setwarnf(m_state, &State::warnFunctionTrampoline, this);
	} else {
		// Detach: Lua disables the warning system entirely until a new
		// function is installed. Matches lua_setwarnf(L, NULL, NULL).
		m_warningsEnabled = false;
		lua_setwarnf(m_state, nullptr, nullptr);
	}
}

void State::warnFunctionTrampoline(void* ud, const char* msg, int tocont) {
	if (!ud || !msg) return;
	static_cast<State*>(ud)->handleWarning(msg, tocont);
}

void State::handleWarning(const char* msg, int tocont) {
	m_warningBuffer.append(msg);
	if (tocont) return; // more pieces follow; wait for the terminal call

	// Take ownership of the accumulated message, then reset the buffer so
	// the next warning starts fresh even if the logger throws.
	std::string assembled;
	assembled.swap(m_warningBuffer);

	// Control directives: leading '@' is reserved by Lua's warning protocol.
	// The two we honor are @on/@off; anything else (e.g. an editor-specific
	// pragma) is silently dropped, matching the default Lua warn function.
	if (!assembled.empty() && assembled.front() == '@') {
		if      (assembled == "@on")  m_warningsEnabled = true;
		else if (assembled == "@off") m_warningsEnabled = false;
		return;
	}

	if (m_warningsEnabled && m_warningLogger) {
		m_warningLogger->log(assembled);
	}
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
	if (m_errorLogger)  m_errorLogger->log(err);
	if (m_errorHandler) (*m_errorHandler)(err);
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

State::~State() {
	if (!m_externalState) {
		lua_close(m_state);
		auto res = s_debugHooks.find(m_state);
		if (res != s_debugHooks.end()) {
			s_debugHooks.erase(res);
		}
	}
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
	m_callbacks.push_back(method);
	registerNativeFunctionWithUpvalues(name, dispatchMethod, m_callbacks.size() - 1, this);
}

void State::registerDebugHook(DebugHook hook, int mask, int count) {
	s_debugHooks[m_state] = hook;

	// The C hook function
	auto chook = [](lua_State* L, lua_Debug* ar) {
		auto res = s_debugHooks.find(L);
		if (res != s_debugHooks.end()) {
			State state(L);
			res->second(state, reinterpret_cast<const DebugInfo&>(*ar));
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
	return reportStatus(LuaError::Category::Runtime, lua_pcall(m_state, 0, LUA_MULTRET, 0));
}

LuaError::Status State::loadAndExecuteScript(const File& path) {
	// Share Registry::loadFile so path-encoding handling (notably non-ASCII
	// paths on Windows) lives in exactly one place. We do NOT use luaL_dofile
	// because that macro expands to (load || pcall), collapsing every non-zero
	// status to 1 — preserving LUA_ERRFILE / LUA_ERRSYNTAX / LUA_ERRRUN is the
	// entire point of the file-loading overload.
	const auto loadRc = reportStatus(LuaError::Category::Load, Registry::loadFile(m_state, path));
	if (loadRc != LuaError::Status::Ok) return loadRc;
	return reportStatus(LuaError::Category::Runtime, lua_pcall(m_state, 0, LUA_MULTRET, 0));
}

Type State::getType(int index) const {
	return static_cast<Type>(lua_type(m_state, index));
}

Type State::pushGlobalToStack(const char* name) {
	return static_cast<Type>(lua_getglobal(m_state, name));
}

void State::setGlobalFromStack(const char* name) {
	lua_setglobal(m_state, name);
}

int State::getStackSize() const {
	return lua_gettop(m_state);
}

void State::withTableDo(std::string_view tableName, TableFunction workOnTable, bool createIfMissing) {
	const bool isTable = (lua_getglobal(m_state, tableName.data()) == LUA_TTABLE);
	// Governs the single value lua_getglobal pushed; on the create path the
	// non-table value is replaced by a fresh table, still a net of one value.
	DefaultStackGuard guard(m_state);

	if (!isTable) {
		if (!createIfMissing) {
			return; // guard pops the non-table value (previously the nil leaked here)
		}
		lua_pop(m_state, 1);                       // drop the non-table value
		lua_newtable(m_state);                     // fresh table (now governed by guard)
		lua_pushvalue(m_state, -1);                // dup, because setglobal pops
		lua_setglobal(m_state, tableName.data());  // consumes the dup
	}

	Table table(m_state, -1); // the table is on top of the stack
	workOnTable(table);       // may throw — guard pops the table
}

void State::withTableDo(int index, TableFunction workOnTable) {
	if (lua_istable(m_state, index)) {
		Table table(m_state, index); //the table is on top of the stack
		workOnTable(table);
	}
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
	return lua_pcall(m_state, numArgs, numResults, 0);
}



} // namespace Lua