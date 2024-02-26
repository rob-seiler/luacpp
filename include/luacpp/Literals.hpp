#ifndef LUA_LITERALS_HPP
#define LUA_LITERALS_HPP

#include <string>

namespace Lua {

std::string operator "" _load(const char* path, std::size_t);

} //namespace Lua

#endif