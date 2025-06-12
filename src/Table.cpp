#include <Table.hpp>
#include <Basics.hpp>
#include <Stack.hpp>

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

std::map<Generic, Generic> Table::readGeneric() {
	std::map<Generic, Generic> result;

	Basics::pushNil(m_state);  // Push a nil key to start the iteration
	while (getNext() != 0) {
		try {
			Generic k = Generic::fromStack(-2, m_state);
			result[k] = Generic::fromStack(-1, m_state);
		} catch (...) {
			Basics::popStack(m_state, 2);  // Pop the key and value from the stack
			throw;  // Rethrow the exception
		}

		Basics::popStack(m_state, 1);  // Pop the value, keep the key for the next iteration
	}

	// No need to pop the table; it remains at the top of the stack
	return result;
}

void Table::writeGeneric(const std::map<Generic, Generic>& map) {
	for (const auto& [key, value] : map) {
		pushToStack(m_state, key);
		pushToStack(m_state, value);
		if (m_triggerMetaMethods) {
			setTable(m_state, m_tableIndex);
		} else {
			setTableRaw(m_state, m_tableIndex);
		}
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