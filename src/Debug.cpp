#include <Debug.hpp>
#include <lua/lua.hpp>

namespace Lua {

std::vector<Generic> Debug::readStack(lua_State* state) {
    std::vector<Generic> stack;
    const int stackSize = lua_gettop(state);
    for (int i = 1; i <= stackSize; ++i) {
        stack.push_back(Generic::fromStack(i, state));
    }
    return stack;
}

} // namespace Lua