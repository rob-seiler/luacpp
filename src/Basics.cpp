#include <Basics.hpp>
#include <lua/lua.hpp>

namespace Lua {

bool Basics::isOfType(lua_State* state, Type type, int index) {
	switch (type) {
		case Type::Nil: return lua_isnil(state, index);
		case Type::Boolean:	return lua_isboolean(state, index);
		case Type::LightUserData: return lua_islightuserdata(state, index);
		case Type::Number: return lua_isnumber(state, index);
		case Type::String: return lua_isstring(state, index);
		case Type::Table: return lua_istable(state, index);
		case Type::Function: return lua_isfunction(state, index);
		case Type::UserData: return lua_isuserdata(state, index);
		case Type::Thread: return lua_isthread(state, index);
		case Type::None: return false;
	}
	return false;
}
bool Basics::isFunction(lua_State* state, int index) {
	return lua_isfunction(state, index);
}

Type Basics::getType(lua_State* state, int index) {
	return static_cast<Type>(lua_type(state, index));
}

void Basics::insert(lua_State* state, int index) {
	lua_insert(state, index);
}

void Basics::popStack(lua_State* state, int numValues) {
	lua_pop(state, numValues);
}

Type Basics::pushGlobal(lua_State* state, const char* name) {
	return static_cast<Type>(lua_getglobal(state, name));
}

void Basics::setGlobal(lua_State* state, const char* name) {
	lua_setglobal(state, name);
}

int  Basics::getStackTop(lua_State* state)              { return lua_gettop(state); }
void Basics::setStackTop(lua_State* state, int newTop)  { lua_settop(state, newTop); }

void Basics::pushNil(lua_State* state) { lua_pushnil(state); }
void Basics::pushBoolean(lua_State* state, bool value) { lua_pushboolean(state, value); }
void Basics::pushNumber(lua_State* state, double value) { lua_pushnumber(state, value); }
void Basics::pushInteger(lua_State* state, int64_t value) { lua_pushinteger(state, value); }
void Basics::pushString(lua_State* state, const char* value) { lua_pushstring(state, value); }
void Basics::pushString(lua_State* state, const char* value, size_t len) { lua_pushlstring(state, value, len); }

void Basics::pushExternalString(lua_State* state, const char* value, size_t len,
                                ExternalStringDeallocator dealloc, void* ud) {
	lua_pushexternalstring(state, value, len, dealloc, ud);
}

void Basics::pushCFunction(lua_State* state, NativeFunction value) { lua_pushcfunction(state, value); }
void Basics::pushLightUserData(lua_State* state, void* value) { lua_pushlightuserdata(state, value); }

bool Basics::isInteger(lua_State* state, int index) { return lua_isinteger(state, index); }

void* Basics::asUserData(lua_State* state, int index) { return lua_touserdata(state, index); }

void* Basics::checkUserData(lua_State* state, int index, const char* tname) {
	return luaL_checkudata(state, index, tname);
}

void* Basics::testUserData(lua_State* state, int index, const char* tname) {
	return luaL_testudata(state, index, tname);
}

bool Basics::asBoolean(lua_State* state, int index) { return lua_toboolean(state, index) != 0; }
double Basics::asNumber(lua_State* state, int index) { return lua_tonumber(state, index); }
int64_t Basics::asInteger(lua_State* state, int index) { return lua_tointeger(state, index); }
const char* Basics::asString(lua_State* state, int index, size_t* len) { return lua_tolstring(state, index, len); }

void* Basics::allocateUserData(lua_State* state, size_t size, int userValues) {
	return lua_newuserdatauv(state, size, userValues);
}

int Basics::calcUpValueIndex(int index) { return lua_upvalueindex(index); }

int Basics::error(lua_State* state, const char* message) {
	return luaL_error(state, "%s", message);
}

} //namespace Lua