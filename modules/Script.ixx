module;
#include <LuaScript.hpp>
#include "../src/LuaScript.cpp"

export module luacpp.Script;

export {
	using Lua::LuaScript;
}