#include <detail/StateVariables.hpp>
#include <lua/lua.hpp>

namespace Lua {

std::map<Generic, Generic> Variables::readTableGeneric(const char* tableName) {
	std::map<Generic, Generic> result;
	const Type t = Basics::pushGlobal(m_state, tableName);
	DefaultStackGuard guard(m_state); // pops on every path, incl. readGeneric throwing
	if (t == Type::Table) {
		Table table(m_state, -1);
		result = table.readGeneric();
	}
	return result;
}

void Variables::withTableDo(std::string_view tableName, TableFunction workOnTable, bool createIfMissing) {
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

void Variables::withTableDo(int index, TableFunction workOnTable) {
	if (lua_istable(m_state, index)) {
		Table table(m_state, index); // the table is on top of the stack
		workOnTable(table);
	}
}

} // namespace Lua
