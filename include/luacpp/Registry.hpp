#ifndef LUACPP_REGISTRY_HPP
#define LUACPP_REGISTRY_HPP

#include <string>

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

	ErrorCode loadScript(const char* name, const char* src);

private:
	lua_State* m_state;
};

} // namespace Lua

#endif // LUACPP_REGISTRY_HPP