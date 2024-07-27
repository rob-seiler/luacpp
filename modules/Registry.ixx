module;
#include <Registry.hpp>
#include "../src/Registry.cpp"

export module luacpp.Registry;

export {
	using Lua::Registry;
}