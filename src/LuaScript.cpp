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

LuaScript::Type LuaScript::getType(int index) const {
	return static_cast<Type>(lua_type(m_state, index));
}

LuaScript::Type LuaScript::pushGlobalToStack(const char* name) {
	return static_cast<Type>(lua_getglobal(m_state, name));
}

void LuaScript::setGlobalFromStack(const char* name) {
	lua_setglobal(m_state, name);
}

int LuaScript::getStackSize() const {
	return lua_gettop(m_state);
}

void LuaScript::withTableDo(std::string_view tableName, TableFunction workOnTable) {
	if (lua_getglobal(m_state, tableName.data()) == LUA_TTABLE) {
		LuaTable table(m_state, -1); //the table is on top of the stack
		workOnTable(table);
	}
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

bool LuaScript::loadFunction(const char* funcName) { 
	return lua_getglobal(m_state, funcName) == LUA_TFUNCTION;
}

int LuaScript::callFunction(int numArgs, int numResults) {
	return lua_pcall(m_state, numArgs, numResults, 0);
}



} // namespace Lua