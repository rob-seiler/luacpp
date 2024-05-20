#ifdef USE_CPP20_MODULES
import luacpp.Basics;
#else
#include <LuaTable.hpp>
#include <Basics.hpp>
#endif

#include <lua/lua.hpp>
#include <stdexcept>

namespace Lua {

LuaTable::LuaTable(lua_State* state, int index, bool triggerMetaMethods)
: m_state(state), m_tableIndex(lua_absindex(state, index)), m_triggerMetaMethods(triggerMetaMethods)
{
	if (!Basics::isOfType(state, Type::Table, index)) {
		throw std::runtime_error("Not implemented");
	}
}

void LuaTable::withTableDo(std::string_view tableName, std::function<void(LuaTable&)> workOnTable) {
	if (lua_getfield(m_state, -1, tableName.data()) == LUA_TTABLE) {
		LuaTable table(m_state, -1); //the table is on top of the stack
		workOnTable(table);
	}
	lua_pop(m_state, 1);
}

bool LuaTable::assignMetaTable(const char* name) {
	if (luaL_getmetatable(m_state, name) == LUA_TTABLE) {
		//stack assumption:
		//-1: metatable
		//-2: table to assign the metatable to
		lua_setmetatable(m_state, -2);
		return true;
	}
	return false;
}

void LuaTable::applyKeyForValue(lua_State* state, const char* key, bool triggerMetaMethods) {
	//assumption of stack layout:
	//-1: value
	//-2: table

	if (triggerMetaMethods) {
		lua_pushstring(state, key);
		lua_insert(state, -2); //move the key below the value
		lua_rawset(state, -3);
	} else {
		lua_setfield(state, -2, key);
	}
}

Type LuaTable::getField(lua_State* state, int idx, const char* key) {
	return static_cast<Type>(lua_getfield(state, idx, key));
}

void LuaTable::setTable(lua_State* state, int idx, bool triggerMetaMethods) {
	if (triggerMetaMethods) {
		lua_settable(state, idx);
	} else {
		lua_rawset(state, idx);
	}
}

int LuaTable::getNext(lua_State* state, int idx) {
	return lua_next(state, idx);
}

} // namespace Lua