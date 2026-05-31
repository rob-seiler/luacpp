#ifndef LUACPP_REGISTRY_HPP
#define LUACPP_REGISTRY_HPP

#include "Basics.hpp"
#include "ErrorHandling.hpp"
#include "Generic.hpp"
#include "Table.hpp"
#include "Stack.hpp"

#include <string>
#include <map>
#include <filesystem>

struct lua_State;

namespace Lua {

class Registry : public Table {
public:
	Registry(lua_State* L);

	LuaError::Status loadScript(Generic key, const char* src);
	LuaError::Status loadScriptFromFile(Generic key, const std::filesystem::path& path);

	template <typename T>
	LuaError::Status loadScript(T key, const char* src) {
		LuaError::Status res = loadString(m_state, src);
		if (res == LuaError::Status::Ok) {
			Stack<T>::push(m_state, key);
			Basics::insert(m_state, -2);
			setTableRaw(m_state, m_tableIndex);
		}
		return res;
	}

	template <typename T>
	LuaError::Status loadScriptFromFile(T key, const std::filesystem::path& path) {
		LuaError::Status res = loadFile(m_state, path);
		if (res == LuaError::Status::Ok) {
			Stack<T>::push(m_state, key);
			Basics::insert(m_state, -2);
			setTableRaw(m_state, m_tableIndex);
		}
		return res;
	}

	LuaError::Status getScript(Generic key);

	template <typename T>
	LuaError::Status getScript(T key) {
		if (getElement(key) != Type::Function) {
			Basics::popStack(m_state, 1);
			return LuaError::Status::RuntimeError;
		}
		return LuaError::Status::Ok;
	}

	bool copyContent(Registry& other);

	// Public so State::loadAndExecuteScript(File) can share the same file-read
	// implementation — see Registry.cpp for why we don't delegate to
	// luaL_loadfile directly.
	static LuaError::Status loadFile(lua_State* state, const std::filesystem::path& path);

private:
	static LuaError::Status loadString(lua_State* state, const char* src);
	static bool isUserDefinedEntry(const Registry& registry);
	static void copyEntry(lua_State* src, lua_State* dst);
};

} // namespace Lua

#endif // LUACPP_REGISTRY_HPP
