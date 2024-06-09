#include <Generic.hpp>

namespace Lua {

Generic::Generic() : m_value(nullptr), m_type(Type::Nil) {}

std::string Generic::toString() const {
	switch (m_type) {
		case Type::Boolean: return std::get<bool>(m_value) ? "true" : "false";
		case Type::LightUserData: return "lightuserdata";
		case Type::Number: 
			if (std::holds_alternative<int64_t>(m_value)) {
				return std::to_string(std::get<int64_t>(m_value));
			} else {
				return std::to_string(std::get<double>(m_value));
			}
		case Type::String: return std::get<std::string>(m_value);
		case Type::Nil:
		case Type::Table:
		case Type::Function:
		case Type::UserData:
		case Type::Thread: 
		case Type::None:
			break;
	}
	return Lua::toString(m_type);
}

Generic Generic::fromStack(int index, lua_State* state) {
	Generic generic;
	switch (Basics::getType(state, index)) {
		case Type::Nil: 
			generic.set(nullptr);
			break;
		case Type::Boolean:
			generic.set(Basics::asBoolean(state, index));
			break;
		case Type::Number: {
			if (Basics::isInteger(state, index)) {
				generic.set(Basics::asInteger(state, index));
			} else {
				generic.set(Basics::asNumber(state, index));
			}
			break;
		}
		case Type::String: {
			size_t len;
			generic.set(std::string(Basics::asString(state, index, &len)));
			break;
		}
		case Type::LightUserData: 
			generic.set(Basics::asUserData(state, index));
			break;
		case Type::Table:
		case Type::Function:
		case Type::UserData:
		case Type::Thread:
		case Type::None:
			break;
	}
	return generic;
}

} // namespace Lua