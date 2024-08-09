module;
#include <Version.hpp>
#include "../src/Version.cpp"

export module luacpp.Version;

export {
	using Lua::Version;
    constexpr const Lua::Version LuaCppVersion{0, 1, 0};
}