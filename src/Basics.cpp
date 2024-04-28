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
	}
	return false;
}

void Basics::popStack(lua_State* state, int numValues) {
	lua_pop(state, numValues);
}

void Basics::pushNil(lua_State* state) { lua_pushnil(state); }
void Basics::pushBoolean(lua_State* state, bool value) { lua_pushboolean(state, value); }
void Basics::pushNumber(lua_State* state, double value) { lua_pushnumber(state, value); }
void Basics::pushInteger(lua_State* state, int64_t value) { lua_pushinteger(state, value); }
void Basics::pushString(lua_State* state, const char* value) { lua_pushstring(state, value); }
void Basics::pushString(lua_State* state, const char* value, size_t len) { lua_pushlstring(state, value, len); }
void Basics::pushCFunction(lua_State* state, NativeFunction value) { lua_pushcfunction(state, value); }
void Basics::pushLightUserData(lua_State* state, void* value) { lua_pushlightuserdata(state, value); }

void* Basics::asUserData(lua_State* state, int index) { return lua_touserdata(state, index); }
bool Basics::asBoolean(lua_State* state, int index) { return lua_toboolean(state, index) != 0; }
double Basics::asNumber(lua_State* state, int index) { return lua_tonumber(state, index); }
int64_t Basics::asInteger(lua_State* state, int index) { return lua_tointeger(state, index); }
const char* Basics::asString(lua_State* state, int index, size_t* len) { return lua_tolstring(state, index, len); }

void* Basics::allocateUserData(lua_State* state, size_t size, int userValues) {
	return lua_newuserdatauv(state, size, userValues);
}

int Basics::calcUpValueIndex(int index) { return lua_upvalueindex(index); }

} //namespace Lua