#ifndef GENERIC_HPP
#define GENERIC_HPP

#ifdef USE_CPP20_MODULES
import luacpp.Type;
import luacpp.Basics;
#else
#include "Type.hpp"
#include "Basics.hpp"
#endif

#include <variant>
#include <string>

struct lua_State;

namespace Lua {

class Generic {
public:
	using Value = std::variant<std::nullptr_t, bool, int64_t, double, std::string, void*>;

	Generic();

	template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
	Generic(T val) : m_value(static_cast<int64_t>(val)), m_type(Type::Number) { }
	Generic(bool val) : m_value(val), m_type(Type::Boolean) { }
	Generic(float val) : m_value(static_cast<double>(val)), m_type(Type::Number) { }
	Generic(double val) : m_value(val), m_type(Type::Number) { }
	Generic(const char* val) : m_value(std::string(val)), m_type(Type::String) { }
	Generic(const std::string& val) : m_value(val), m_type(Type::String) { }
	Generic(void* val) : m_value(val), m_type(Type::LightUserData) { }
	Generic(std::nullptr_t) : m_value(nullptr), m_type(Type::Nil) { }
	Generic(const Generic& other) : m_value(other.m_value), m_type(other.m_type) { }

	template <typename T, typename = std::enable_if_t<!std::is_integral<T>::value || std::is_same<T, int64_t>::value || std::is_same<T, bool>::value>>
    T get() const { return std::get<T>(m_value); }

    template <typename T, typename = void, typename = std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, int64_t>::value && !std::is_same<T, bool>::value>>
    T get() const { return static_cast<T>(std::get<int64_t>(m_value)); }

	template <typename T>
	void set(T val) {
		m_value = val;
		m_type = Basics::getTypeFor<T>();
	}

	Type getType() const { return m_type; }

	std::string toString() const;

	static Generic fromStack(int index, lua_State* state);

	bool operator==(const Generic& other) const;
	bool operator<(const Generic& other) const;

private:
	Value m_value;
	Type m_type;
};

} // namespace Lua

#endif // GENERIC_HPP