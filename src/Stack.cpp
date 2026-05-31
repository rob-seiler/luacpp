#include <Stack.hpp>

#include <Basics.hpp>
#include <Generic.hpp>

namespace Lua {

void Stack<const std::string&>::push(lua_State* state, const std::string& value) {
	Basics::pushString(state, value.c_str(), value.length());
}

void Stack<Generic>::push(lua_State* state, Generic value) {
	Generic::toStack(value, state);
}

} // namespace Lua