#ifndef LUACPP_TYPEMISMATCHEXCEPTION_HPP
#define LUACPP_TYPEMISMATCHEXCEPTION_HPP

#ifdef USE_CPP20_MODULES
import luacpp.Type;
#else
#include "Type.hpp"
#endif

#include <exception>
#include <string>

#if __has_include(<version>)
	#include <version>
	#ifdef __cpp_lib_format
		#include <format>
	#endif
#endif

namespace Lua {

class TypeMismatchException : public std::exception {
public:
    TypeMismatchException(const Type expected, const Type actual, const char* varName)
	: m_expected(expected), m_actual(actual), m_varName(varName) {}

	const char* what() const noexcept override {
		if (m_message.empty()) {
			generateMessage(m_message);
		}
		return m_message.c_str();
	}

	Type getExpectedType() const { return m_expected; }
	Type getActualType() const { return m_actual; }

private:
#ifdef __cpp_lib_format
	void generateMessage(std::string& str) const {
		if (!m_varName.empty()) {
			str = std::format("{}: ", m_varName);
		}
		str += std::format("Expected type: {}, but got: {}", toString(m_expected), toString(m_actual));
	}
#else
	void generateMessage(std::string& str) const {
		if (!m_varName.empty()) {
			str = m_varName + ": ";
		}
		str += "Expected type: " + std::string(toString(m_expected)) + ", but got: " + toString(m_actual);
	}
#endif
    const Type m_expected;
    const Type m_actual;
	const std::string m_varName;
	mutable std::string m_message;
};

} // namespace Lua

#endif // LUACPP_TYPEMISMATCHEXCEPTION_HPP