#ifndef LUACPP_LUATABLE_HPP
#define LUACPP_LUATABLE_HPP

#include "Basics.hpp"
#include <string_view>
#include <functional>
#include <map>

struct lua_State;

namespace Lua {

class LuaTable {
public:
	using Type = Basics::Type;
	LuaTable(lua_State* state, int index = -1, bool triggerMetaMethods = false);

	template <typename Key, typename Value>
	void setElement(Key key, Value value) {
		Basics::pushToStack<Key>(m_state, key);
		Basics::pushToStack<Value>(m_state, value);
		setTable(m_state, -3, m_triggerMetaMethods);
	}

	template <typename T>
	bool readValue(std::string_view key, T& value) {
		Type valType = getField(m_state, m_tableIndex, key.data());
		const bool retVal = Basics::getTypeFor<T>() == valType;
		if (retVal) {
			value = Basics::getStackValue<T>(m_state, -1);
		}
		Basics::popStack(m_state, 1);
		return retVal;
	}

	template <typename Key, typename Value>
	std::map<Key, Value> read() {
		std::map<Key, Value> result;

		Basics::pushNil(m_state);  // Push a nil key to start the iteration
		while (getNext() != 0) {
			if (!Basics::isOfType(m_state, Basics::getTypeFor<Key>(), -2)) {
				throw std::runtime_error("Key is not of the expected type");
			}
			if (!Basics::isOfType(m_state, Basics::getTypeFor<Value>(), -1)) {
				throw std::runtime_error("Value is not of the expected type");
			}

			Key k = Basics::getStackValue<Key>(m_state, -2);
			Value v = Basics::getStackValue<Value>(m_state, -1);
			result[k] = v;

			Basics::popStack(m_state, 1);  // Pop the value, keep the key for the next iteration
		}

		// No need to pop the table; it remains at the top of the stack
		return result;
	}

    template <typename Key, typename Value>
    void write(const std::map<Key, Value>& map) {
        for (const auto& [key, value] : map) {
			setElement(key, value);
        }
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

	static void setTable(lua_State* state, int idx, bool triggerMetaMethods);

	static int getNext(lua_State* state, int idx);

	int getNext() { return getNext(m_state, m_tableIndex); }

	lua_State* m_state;
	int m_tableIndex;
	bool m_triggerMetaMethods;
};

} // namespace Lua

#endif //LUACPP_LUATABLE_HPP