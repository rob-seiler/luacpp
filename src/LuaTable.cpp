#include <LuaTable.hpp>
#include <LuaScript.hpp>
#include <Basics.hpp>
#include <lua/lua.hpp>

namespace Lua {

LuaTable::LuaTable(lua_State* state, int index, bool rawset)
: m_state(state), m_tableIndex(index), m_setRaw(rawset)
{
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

void LuaTable::applyKeyForValue(lua_State* state, const char* key, bool rawset) {
	//assumption of stack layout:
	//-1: value
	//-2: table

	if (rawset) {
		lua_pushstring(state, key);
		lua_insert(state, -2); //move the key below the value
		lua_rawset(state, -3);
	} else {
		lua_setfield(state, -2, key);
	}
}

LuaTable::Type LuaTable::getField(lua_State* state, int idx, const char* key) {
	return static_cast<Type>(lua_getfield(state, idx, key));
}

} // namespace Lua