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

	template <typename T>
	Generic(T val) : m_value(val), m_type(Basics::getTypeFor<T>()) {}

	template <typename T>
	T get() const {
		return std::get<T>(m_value);
	}

	template <typename T>
	void set(T val) {
		m_value = val;
		m_type = Basics::getTypeFor<T>();
	}

	Type getType() const { return m_type; }

	std::string toString() const;

	static Generic fromStack(int index, lua_State* state);

private:
	Value m_value;
	Type m_type;
};

} // namespace Lua

#endif // GENERIC_HPP