module;
#include <Type.hpp>

export module luacpp.Type;

export {
	using Lua::Type;
	constexpr const char* Lua::toString(Lua::Type t);
}