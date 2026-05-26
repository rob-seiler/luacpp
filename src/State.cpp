#include <State.hpp>
#include <lua/lua.hpp>

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
} // namespace

namespace Lua {

std::map<lua_State*, State::DebugHook> State::s_debugHooks;

State::State(Library libraries)
: m_state(luaL_newstate()),
  m_registry(m_state),
  m_externalState(false),
  m_errorHandler(std::make_unique<NullHandler>())
{
	openLibrary(libraries);
}

State::State(lua_State* state)
: m_state(state),
  m_registry(state),
  m_externalState(true),
  m_errorHandler(std::make_unique<NullHandler>())
{
}

void State::setErrorHandler(std::unique_ptr<ErrorHandler> handler) {
	// Keep the invariant that m_errorHandler is never null after construction
	// — callers can reset by passing nullptr explicitly, which collapses back
	// to the silent default.
	m_errorHandler = handler ? std::move(handler) : std::make_unique<NullHandler>();
}

void State::reportError(LuaError::Category category, int status) {
	LuaError err{category, status, {}, {}};
	if (lua_isstring(m_state, -1)) {
		err.message = lua_tostring(m_state, -1);
		lua_pop(m_state, 1);
	}
	(*m_errorHandler)(err);
}

void State::reportError(LuaError err) {
	(*m_errorHandler)(err);
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
	// Pop guard: ensure the package table (or the placeholder for it) is
	// off the stack on every exit path.
	if (lua_getglobal(m_state, "package") != LUA_TTABLE) {
		lua_pop(m_state, 1);
		return; // LibPackage not loaded — no package table to extend
	}

	const char* fieldName = forNativeModule ? "cpath" : "path";
	lua_getfield(m_state, -1, fieldName);
	size_t currentLen = 0;
	const char* current = lua_tolstring(m_state, -1, &currentLen);

	std::string combined;
	combined.reserve(pattern.size() + 1 + currentLen);
	combined.append(pattern);
	if (current && currentLen > 0) {
		combined.push_back(';');
		combined.append(current, currentLen);
	}

	lua_pop(m_state, 1); // pop old path/cpath
	lua_pushlstring(m_state, combined.data(), combined.size());
	lua_setfield(m_state, -2, fieldName);
	lua_pop(m_state, 1); // pop package table
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
	lua_pushcclosure(m_state, func, 0); //push function to stack
	lua_setfield(m_state, -2, name); //register the function under the given name
	lua_pop(m_state, 1); //pop global scope
}

void State::loadAndExecuteScript(const char* code) {
	// Split load/exec explicitly (instead of luaL_dostring) so that load
	// failures and runtime failures land in distinct LuaError categories.
	int status = luaL_loadstring(m_state, code);
	if (status != LUA_OK) {
		reportError(LuaError::Category::Load, status);
		return;
	}
	status = lua_pcall(m_state, 0, LUA_MULTRET, 0);
	if (status != LUA_OK) {
		reportError(LuaError::Category::Runtime, status);
	}
}

void State::loadAndExecuteScript(const File& path) {
	// Share Registry::loadFile so path-encoding handling (notably non-ASCII
	// paths on Windows) lives in exactly one place. We do NOT use luaL_dofile
	// because that macro expands to (load || pcall), collapsing every non-zero
	// status to 1 — preserving LUA_ERRFILE / LUA_ERRSYNTAX / LUA_ERRRUN is the
	// entire point of the file-loading overload.
	int status = static_cast<int>(Registry::loadFile(m_state, path));
	if (status != LUA_OK) {
		reportError(LuaError::Category::Load, status);
		return;
	}
	status = lua_pcall(m_state, 0, LUA_MULTRET, 0);
	if (status != LUA_OK) {
		reportError(LuaError::Category::Runtime, status);
	}
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
	if (lua_getglobal(m_state, tableName.data()) != LUA_TTABLE) {
		if (createIfMissing) {
			lua_newtable(m_state); // Create a new table and push it onto the stack
			lua_pushvalue(m_state, -1); // Duplicate the table because setglobal pops the value
			lua_setglobal(m_state, tableName.data()); // Set the new table as a global variable
		} else {
			lua_pop(m_state, 1); // Pop the nil from the stack to clean up
			return; // Exit the function as there's no table to work with and creation is not requested
		}
	}

	Table table(m_state, -1); //the table is on top of the stack
	workOnTable(table);
	lua_pop(m_state, 1);
}

void State::withTableDo(int index, TableFunction workOnTable) {
	if (lua_istable(m_state, index)) {
		Table table(m_state, index); //the table is on top of the stack
		workOnTable(table);
	}
}

void State::createTable(const char* name, TableFunction workOnTable) {
	lua_newtable(m_state);
	Table table(m_state, -1); //the table is on top of the stack
	workOnTable(table);
	if (name != nullptr) {
		lua_setglobal(m_state, name);
	}
}

void State::createMetaTable(const char* name, TableFunction workOnTable) {
	luaL_newmetatable(m_state, name);
	Table table(m_state, -1, true); //the table is on top of the stack
	workOnTable(table);
	lua_pop(m_state, 1);
}

bool State::assignMetaTable(const char* name) {
	if (luaL_getmetatable(m_state, name) == LUA_TTABLE) {
		//stack assumption:
		//-1: metatable
		//-2: userdata to assign the metatable to
		lua_setmetatable(m_state, -2);
		return true;
	}
	return false;
}

int State::dispatchMethod(lua_State* state) {
	const int32_t index = static_cast<int32_t>(lua_tointeger(state, lua_upvalueindex(1)));
	State* luaState = static_cast<State*>(lua_touserdata(state, lua_upvalueindex(2)));
	return luaState->m_callbacks[index](*luaState);
}

bool State::loadFunction(const char* funcName) { 
	return lua_getglobal(m_state, funcName) == LUA_TFUNCTION;
}

int State::callFunction(int numArgs, int numResults) {
	return lua_pcall(m_state, numArgs, numResults, 0);
}



} // namespace Lua