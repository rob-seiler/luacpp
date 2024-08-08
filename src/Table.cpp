#ifdef USE_CPP20_MODULES
import luacpp.Basics;
#else
#include <Table.hpp>
#include <Basics.hpp>
#endif

#include <lua/lua.hpp>
#include <stdexcept>

namespace Lua {

Table::Table(lua_State* state, int index, bool triggerMetaMethods)
: m_state(state), m_tableIndex(lua_absindex(state, index)), m_triggerMetaMethods(triggerMetaMethods)
{
	if (!Basics::isOfType(state, Type::Table, index)) {
		throw std::runtime_error("Not implemented");
	}
}

void Table::withTableDo(std::string_view tableName, std::function<void(Table&)> workOnTable) {
	if (lua_getfield(m_state, -1, tableName.data()) == LUA_TTABLE) {
		Table table(m_state, -1); //the table is on top of the stack
		workOnTable(table);
	}
	lua_pop(m_state, 1);
}

bool Table::assignMetaTable(const char* name) {
	if (luaL_getmetatable(m_state, name) == LUA_TTABLE) {
		//stack assumption:
		//-1: metatable
		//-2: table to assign the metatable to
		lua_setmetatable(m_state, -2);
		return true;
	}
	return false;
}

void Table::applyKeyForValue(lua_State* state, const char* key, bool triggerMetaMethods) {
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

Type Table::getField(lua_State* state, int idx, const char* key) {
	return static_cast<Type>(lua_getfield(state, idx, key));
}

Type Table::getTable(lua_State* state, int idx) {
	return static_cast<Type>(lua_gettable(state, idx));
}
Type Table::getTableRaw(lua_State* state, int idx) {
	return static_cast<Type>(lua_rawget(state, idx));
}

void Table::setTable(lua_State* state, int idx) {
	lua_rawset(state, idx);
}

void Table::setTableRaw(lua_State* state, int idx) {
	lua_rawset(state, idx);
}

int Table::getNext(lua_State* state, int idx) {
	return lua_next(state, idx);
}

} // namespace Lua