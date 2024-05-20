module;
#include <LuaTable.hpp>
#include "../src/LuaTable.cpp"

export module luacpp.Table;

export {
	using Lua::LuaTable;
}