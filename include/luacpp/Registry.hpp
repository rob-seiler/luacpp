#ifndef LUACPP_REGISTRY_HPP
#define LUACPP_REGISTRY_HPP

#include "Basics.hpp"
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
	enum class ErrorCode {
		Ok = 0,
		Yield = 1,
		RuntimeError = 2,
		SyntaxError = 3,
		MemoryError = 4,
		ErrorError = 5,
		FileError = 6 ///< file could not be opened (LUA_ERRFILE)
	};

	Registry(lua_State* L);

	ErrorCode loadScript(Generic key, const char* src);
	ErrorCode loadScriptFromFile(Generic key, const std::filesystem::path& path);

	template <typename T>
	ErrorCode loadScript(T key, const char* src) {
		ErrorCode res = loadString(m_state, src);
		if (res == ErrorCode::Ok) {
			Stack<T>::push(m_state, key);
			Basics::insert(m_state, -2);
			setTableRaw(m_state, m_tableIndex);
		}
		return res;
	}

	template <typename T>
	ErrorCode loadScriptFromFile(T key, const std::filesystem::path& path) {
		ErrorCode res = loadFile(m_state, path);
		if (res == ErrorCode::Ok) {
			Stack<T>::push(m_state, key);
			Basics::insert(m_state, -2);
			setTableRaw(m_state, m_tableIndex);
		}
		return res;
	}

	ErrorCode getScript(Generic key);

	template <typename T>
	ErrorCode getScript(T key) {
		if (getElement(key) != Type::Function) {
			Basics::popStack(m_state, 1);
			return ErrorCode::RuntimeError;
		}
		return ErrorCode::Ok;
	}

	bool copyContent(Registry& other);

private:
	static ErrorCode loadString(lua_State* state, const char* src);
	static ErrorCode loadFile(lua_State* state, const std::filesystem::path& path);
	static bool isUserDefinedEntry(const Registry& registry);
	static void copyEntry(lua_State* src, lua_State* dst);
};

} // namespace Lua

#endif // LUACPP_REGISTRY_HPP