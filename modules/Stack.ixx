module;
#include <Stack.hpp>
#include "../src/Stack.cpp"

export module luacpp.Stack;

export {
	using Lua::Stack;
}