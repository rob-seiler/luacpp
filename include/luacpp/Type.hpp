#ifndef LUACPP_TYPE_HPP
#define LUACPP_TYPE_HPP

namespace Lua {

enum class Type : int {
	None = -1,
	Nil = 0,
	Boolean = 1,
	LightUserData = 2,
	Number = 3,
	String = 4,
	Table = 5,
	Function = 6,
	UserData = 7,
	Thread = 8
};

constexpr const char* toString(Type t) {
	switch (t) {
		case Type::None: return "none";
		case Type::Nil: return "nil";
		case Type::Boolean: return "boolean";
		case Type::LightUserData: return "light userdata";
		case Type::Number: return "number";
		case Type::String: return "string";
		case Type::Table: return "table";
		case Type::Function: return "function";
		case Type::UserData: return "userdata";
		case Type::Thread: return "thread";
	}
	return "unknown";
}

} // namespace Lua

#endif // LUACPP_TYPE_HPP