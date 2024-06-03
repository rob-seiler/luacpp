#include <LuaScript.hpp>
#include <lua/lua.hpp>

#include <string>
#include <array>
#include <limits> //std::numeric_limits

namespace {
struct LibraryLoadingFunction {
    int mask;
	const char* name;
	int (*func)(lua_State*);
};
} // namespace

namespace Lua {

std::map<lua_State*, LuaScript::DebugHook> LuaScript::s_debugHooks;

LuaScript::LuaScript(Library libraries) 
: m_state(luaL_newstate()),
  m_externalState(false)
{ 
	openLibrary(libraries);
}

LuaScript::LuaScript(lua_State* state)
: m_state(state),
  m_externalState(true)
{
}

LuaScript::LuaScript(LuaScript&& mv)
: m_state(mv.m_state),
  m_externalState(mv.m_externalState),
  m_errorList(std::move(mv.m_errorList))
{
	mv.m_state = nullptr;
	mv.m_externalState = true;
}

LuaScript::~LuaScript() {
	if (!m_externalState) {
		lua_close(m_state);
		auto res = s_debugHooks.find(m_state);
		if (res != s_debugHooks.end()) {
			s_debugHooks.erase(res);
		}
	}
}

void LuaScript::openLibrary(Library library) {
	static const std::array<LibraryLoadingFunction, LibraryCount> libraries = {
		LibraryLoadingFunction{LibBase, LUA_GNAME, luaopen_base},
		LibraryLoadingFunction{LibPackage, LUA_LOADLIBNAME, luaopen_package},
		LibraryLoadingFunction{LibCoroutine, LUA_COLIBNAME, luaopen_coroutine},
		LibraryLoadingFunction{LibTable, LUA_TABLIBNAME, luaopen_table},
		LibraryLoadingFunction{LibIO, LUA_IOLIBNAME, luaopen_io},
		LibraryLoadingFunction{LibOS, LUA_OSLIBNAME, luaopen_os},
		LibraryLoadingFunction{LibString, LUA_STRLIBNAME, luaopen_string},
		LibraryLoadingFunction{LibMath, LUA_MATHLIBNAME, luaopen_math},
		LibraryLoadingFunction{LibUTF8, LUA_UTF8LIBNAME, luaopen_utf8},
		LibraryLoadingFunction{LibDebug, LUA_DBLIBNAME, luaopen_debug}	
	};

	if (library == LibAll) {
		luaL_openlibs(m_state);
		return;
	}

	for (const auto& lib : libraries) {
		if (library & lib.mask) {
			luaL_requiref(m_state, lib.name, lib.func, 1);
    		lua_pop(m_state, 1);  /* remove lib */
		}
	}
}

int LuaScript::registerNativeFunction(const char* name, NativeFunction func, int numUpValues) {
	lua_pushcclosure(m_state, func, numUpValues);
	lua_setglobal(m_state, name);
	return 0;
}

int LuaScript::registerMethod(const char* name, Method method) {
	m_callbacks.push_back(method);
	return registerNativeFunctionWithUpvalues(name, dispatchMethod, m_callbacks.size() - 1, this);
}

void LuaScript::registerDebugHook(DebugHook hook, int mask, int count) {
	s_debugHooks[m_state] = hook;

	// The C hook function
	auto chook = [](lua_State* L, lua_Debug* ar) {
		auto res = s_debugHooks.find(L);
		if (res != s_debugHooks.end()) {
			LuaScript script(L);
			res->second(script, reinterpret_cast<const DebugInfo&>(*ar));
		}
	};

    lua_sethook(m_state, chook, mask, count);
}

int LuaScript::overrideLuaFunction(const char* name, NativeFunction func) {
	lua_getglobal(m_state, GlobalScope); //load global scope to stack
	lua_pushcclosure(m_state, func, 0); //push function to stack
    lua_setfield(m_state, -2, name); //register the function under the given name
	lua_pop(m_state, 1); //pop global scope
	return 0;
}

int LuaScript::loadScriptIntoGlobal(const char* name, const char* code) {
	int status = luaL_loadstring(m_state, code);
	if (status == LUA_OK) {
		lua_setglobal(m_state, name);
	} else {
		//lua failed to load the script and push an error message on the stack
		//we store the message in our error log and clean up the stack
		while (lua_isstring(m_state, -1)) {
			m_errorList.emplace_back(lua_tostring(m_state, -1));
			lua_pop(m_state, 1);
		}
	}
	return status;
}

int LuaScript::loadAndExecuteScript(const char* code) {
	int status = luaL_dostring(m_state, code);
	if (status != LUA_OK) {
		//lua failed to load the script and push an error message on the stack
		//we store the message in our error log and clean up the stack
		while (lua_isstring(m_state, -1)) {
			m_errorList.emplace_back(lua_tostring(m_state, -1));
			lua_pop(m_state, 1);
		}
	}
	return status;
}

Type LuaScript::getType(int index) const {
	return static_cast<Type>(lua_type(m_state, index));
}

Type LuaScript::pushGlobalToStack(const char* name) {
	return static_cast<Type>(lua_getglobal(m_state, name));
}

void LuaScript::setGlobalFromStack(const char* name) {
	lua_setglobal(m_state, name);
}

int LuaScript::getStackSize() const {
	return lua_gettop(m_state);
}

void LuaScript::withTableDo(std::string_view tableName, TableFunction workOnTable, bool createIfMissing) {
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

	LuaTable table(m_state, -1); //the table is on top of the stack
	workOnTable(table);
	lua_pop(m_state, 1);
}

void LuaScript::withTableDo(int index, TableFunction workOnTable) {
	if (lua_istable(m_state, index)) {
		LuaTable table(m_state, index); //the table is on top of the stack
		workOnTable(table);
	}
}

void LuaScript::createTable(const char* name, TableFunction workOnTable) {
	lua_newtable(m_state);
	LuaTable table(m_state, -1); //the table is on top of the stack
	workOnTable(table);
	if (name != nullptr) {
		lua_setglobal(m_state, name);
	}
}

void LuaScript::createMetaTable(const char* name, TableFunction workOnTable) {
	luaL_newmetatable(m_state, name);
	LuaTable table(m_state, -1, true); //the table is on top of the stack
	workOnTable(table);
	lua_pop(m_state, 1);
}

bool LuaScript::assignMetaTable(const char* name) {
	if (luaL_getmetatable(m_state, name) == LUA_TTABLE) {
		//stack assumption:
		//-1: metatable
		//-2: userdata to assign the metatable to
		lua_setmetatable(m_state, -2);
		return true;
	}
	return false;
}

int LuaScript::dispatchMethod(lua_State* state) {
	const int32_t index = static_cast<int32_t>(lua_tointeger(state, lua_upvalueindex(1)));
	LuaScript* script = static_cast<LuaScript*>(lua_touserdata(state, lua_upvalueindex(2)));
	return script->m_callbacks[index](*script);
}

bool LuaScript::loadFunction(const char* funcName) { 
	return lua_getglobal(m_state, funcName) == LUA_TFUNCTION;
}

int LuaScript::callFunction(int numArgs, int numResults) {
	return lua_pcall(m_state, numArgs, numResults, 0);
}



} // namespace Lua