#include <Generic.hpp>

namespace {

struct Comparator {
    template<typename T>
    bool operator()(const T& lhs, const T& rhs) const {
        return lhs < rhs;
    }

    // Special handling for non-comparable types or types needing special comparison
    bool operator()(const void* lhs, const void* rhs) const { return lhs < rhs; }
	bool operator()(const std::nullptr_t&, const std::nullptr_t&) const { return false; }

    // Fallback for different types
    template<typename T, typename U>
    bool operator()(const T&, const U&) const { return false; }
};

template <>
bool Comparator::operator()(const double& lhs, const int64_t& rhs) const { return lhs < static_cast<double>(rhs); }

template <>
bool Comparator::operator()(const int64_t& lhs, const double& rhs) const { return static_cast<double>(lhs) < rhs; }

} //namespace anonymous

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

bool Generic::isInteger() const { return std::holds_alternative<int64_t>(m_value); }
bool Generic::isDouble() const { return std::holds_alternative<double>(m_value); }

bool Generic::operator==(const Generic& other) const {
	return m_type == other.m_type && m_value == other.m_value;
}

bool Generic::operator<(const Generic& other) const {
	return m_type < other.m_type || (m_type == other.m_type && std::visit(Comparator{}, m_value, other.m_value));
}

Generic& Generic::operator=(const Generic& other) {
	m_value = other.m_value;
	m_type = other.m_type;
	return *this;
}

} // namespace Lua