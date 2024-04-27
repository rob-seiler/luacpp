#ifndef LUACPP_LUATABLE_HPP
#define LUACPP_LUATABLE_HPP

#include "Basics.hpp"
#include <string_view>
#include <functional>

struct lua_State;

namespace Lua {

class LuaTable {
public:
	using Type = Basics::Type;
	LuaTable(lua_State* state, int index = -1, bool rawset = false);

	template <typename T>
	void createElement(const char* key, T value) {
		Basics::pushToStack<T>(m_state, value);
		applyKeyForValue(m_state, key, m_setRaw);
	}

	template <typename T>
	bool readValue(std::string_view key, T& value) {
		Type valType = getField(m_state, m_tableIndex, key.data());
		const bool retVal = Basics::doesTypeMatch<T>(valType);
		if (retVal) {
			value = Basics::getStackValue<T>(m_state, -1);
		}
		Basics::popStack(m_state, 1);
		return retVal;
	}

	/**
	 * \brief work on the nested table with the given name
	 * This method pushes the table with the given name from the table which is currently on the stack onto the stack and calls the given function.
	*/
	void withTableDo(std::string_view tableName, std::function<void(LuaTable&)> workOnTable);

	/**
	 * \brief assign a metatable to the table on top of the stack
	 * This method assumes the table to work on is on top of the stack.
	 * @param name The name of the metatable
	 * @return true if the metatable was found and assigned, false otherwise
	*/
	bool assignMetaTable(const char* name);
	
private:
	/**
	 * @brief applies the given key to the value on top of the stack
	 * 
	 * This method assumes the value to work on is on top of the stack. The second element on the stack is the table to work on.
	 * The value is popped and will be stored together with the given key in the table.
	*/
	static void applyKeyForValue(lua_State* state, const char* key, bool rawset = false);

	static Type getField(lua_State* state, int idx, const char* key);

	lua_State* m_state;
	int m_tableIndex;
	bool m_setRaw;
};

} // namespace Lua

#endif //LUACPP_LUATABLE_HPP