module;
#include <Debug.hpp>
#include "../src/Debug.cpp"

export module luacpp.Debug;

export {
	using Lua::DebugInfo;
	using Lua::EventCodes;

	using Lua::MaskCall;
	using Lua::MaskReturn;
	using Lua::MaskLine;
	using Lua::MaskCount;

	using Lua::Debug;
}