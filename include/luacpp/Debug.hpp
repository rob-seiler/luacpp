#ifndef LUACPP_DEBUG_HPP
#define LUACPP_DEBUG_HPP

#ifdef USE_CPP20_MODULES
import luacpp.Generic;
#else
#include "Generic.hpp"
#endif

#include <vector>

struct lua_State;

namespace Lua {

constexpr static const int srcSize = 60;

struct DebugInfo {
	int event;
	const char* name;
	const char* namewhat;
	const char* what;
	const char* source;
	size_t srclen;
	int currentline;
	int linedefined;
	int lastlinedefined;
	unsigned char nups;
	unsigned char nparams;
	char isvararg;
	char istailcall;
	unsigned short ftransfer;
	unsigned short ntransfer;
	char short_src[srcSize];
	// Ignore the private part
};

/*
** Event codes
*/
enum class EventCodes : int {
	Call = 0,
	Return = 1,
	Line = 2,
	Count = 3,
	TailCall = 4
};

/*
** Event masks
*/
constexpr static const int MaskCall = 1 << static_cast<int>(EventCodes::Call);
constexpr static const int MaskReturn = 1 << static_cast<int>(EventCodes::Return);
constexpr static const int MaskLine = 1 << static_cast<int>(EventCodes::Line);
constexpr static const int MaskCount = 1 << static_cast<int>(EventCodes::Count);

class Debug {
public:
	static std::vector<Generic> readStack(lua_State* state);
};

} // namespace Lua

#endif // LUACPP_DEBUG_HPP