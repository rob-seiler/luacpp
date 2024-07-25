module;
#include <State.hpp>
#include "../src/State.cpp"

export module luacpp.State;

export {
	using Lua::State;
}