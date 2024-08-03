#ifndef LUACPP_REGISTRY_HPP
#define LUACPP_REGISTRY_HPP

#ifdef USE_CPP20_MODULES
import luacpp.Basics;
import luacpp.Generic;
import luacpp.Table;
#else
#include "Basics.hpp"
#include "Generic.hpp"
#include "Table.hpp"
#endif

#include <string>
#include <map>

struct lua_State;

namespace Lua {

class Registry {
public:
	enum class ErrorCode {
		Ok = 0,
		Yield = 1,
		RuntimeError = 2,
		SyntaxError = 3,
		MemoryError = 4,
		ErrorError = 5
	};

	Registry(lua_State* L);

	ErrorCode loadScript(Generic key, const char* src);
	
	template <typename T>
	ErrorCode loadScript(T key, const char* src) {
		ErrorCode res = loadString(m_state, src);
		if (res == ErrorCode::Ok) {
			Basics::pushToStack(m_state, key);
			Basics::insert(m_state, -2);
			setRegistryTable(m_state);
		}
		return res;
	}

	ErrorCode getScript(Generic key);
	
	template <typename T>
	ErrorCode getScript(T key) {
		if (getEntry(key) != Type::Function) {
			Basics::popStack(m_state, 1);
			return ErrorCode::RuntimeError;
		}
		return ErrorCode::Ok;
	}

	template <typename T, typename U>
	void setEntry(T key, U value) {
		Basics::pushToStack(m_state, key);
		Basics::pushToStack(m_state, value);
		setRegistryTable(m_state);
	}

	template <typename T>
	Type getEntry(T key) {
		Basics::pushToStack(m_state, key);
		return getRegistryTable(m_state);
	}

	bool copyContent(Registry& other);

private:
	static ErrorCode loadString(lua_State* state, const char* src);
	static Type getRegistryTable(lua_State* state);
	static void setRegistryTable(lua_State* state);
	static bool isUserDefinedEntry(const Registry& registry);
	static void copyEntry(lua_State* src, lua_State* dst);	

	lua_State* m_state;
};

} // namespace Lua

#endif // LUACPP_REGISTRY_HPP