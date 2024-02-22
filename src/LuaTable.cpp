#include <scripting/LuaTable.hpp>
#include <scripting/LuaScript.hpp>
#include <lua/lua.hpp>

namespace Promess {
namespace UFM6 {

LuaTable::LuaTable(lua_State* state, int index, bool rawset)
: m_state(state), m_tableIndex(index), m_setRaw(rawset)
{
}


template <typename T>
void LuaTable::createElement(const char* key, T value) {
	LuaScript lua(m_state);
	lua.pushToStack<T>(value);
	applyKeyForValue(m_state, key, m_setRaw);
}

template <typename T>
bool LuaTable::readValue(std::string_view key, T& value) {
	LuaScript lua(m_state);
	LuaScript::Type valType = static_cast<LuaScript::Type>(lua_getfield(m_state, m_tableIndex, key.data()));
	const bool retVal = lua.doesTypeMatch<T>(valType);
	if (retVal) {
		value = lua.getStackValue<T>(-1);
	}
	lua.popStack(1);
	return retVal;
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

// Explicit instantiations
template void LuaTable::createElement<void*>(const char*, void*);
template void LuaTable::createElement<bool>(const char*, bool);
template void LuaTable::createElement<int8_t>(const char*, int8_t);
template void LuaTable::createElement<uint8_t>(const char*, uint8_t);
template void LuaTable::createElement<int16_t>(const char*, int16_t);
template void LuaTable::createElement<uint16_t>(const char*, uint16_t);
template void LuaTable::createElement<int32_t>(const char*, int32_t);
template void LuaTable::createElement<uint32_t>(const char*, uint32_t);
template void LuaTable::createElement<int64_t>(const char*, int64_t);
template void LuaTable::createElement<uint64_t>(const char*, uint64_t);
template void LuaTable::createElement<float>(const char*, float);
template void LuaTable::createElement<double>(const char*, double);
template void LuaTable::createElement<const char*>(const char*, const char*);
template void LuaTable::createElement<std::string_view>(const char*, std::string_view);
template void LuaTable::createElement<std::string>(const char*, std::string);
template void LuaTable::createElement<const std::string&>(const char*, const std::string&);
template void LuaTable::createElement<LuaScript::NativeFunction>(const char*, LuaScript::NativeFunction);

template bool LuaTable::readValue<void*>(std::string_view, void*&);
template bool LuaTable::readValue<bool>(std::string_view, bool&);
template bool LuaTable::readValue<int8_t>(std::string_view, int8_t&);
template bool LuaTable::readValue<uint8_t>(std::string_view, uint8_t&);
template bool LuaTable::readValue<int16_t>(std::string_view, int16_t&);
template bool LuaTable::readValue<uint16_t>(std::string_view, uint16_t&);
template bool LuaTable::readValue<int32_t>(std::string_view, int32_t&);
template bool LuaTable::readValue<uint32_t>(std::string_view, uint32_t&);
template bool LuaTable::readValue<int64_t>(std::string_view, int64_t&);
template bool LuaTable::readValue<uint64_t>(std::string_view, uint64_t&);
template bool LuaTable::readValue<float>(std::string_view, float&);
template bool LuaTable::readValue<double>(std::string_view, double&);
template bool LuaTable::readValue<const char*>(std::string_view, const char*&);
template bool LuaTable::readValue<std::string_view>(std::string_view, std::string_view&);
template bool LuaTable::readValue<std::string>(std::string_view, std::string&);

} // namespace UFM6
} // namespace Promess