#include <ErrorHandling.hpp>

#include <iostream>
#include <ostream>

namespace Lua {

StreamLogger::StreamLogger() : m_out(&std::cerr) {}

StreamLogger::StreamLogger(std::ostream& out) noexcept : m_out(&out) {}

void StreamLogger::log(const LuaError& e) {
	const char* cat = (e.category == LuaError::Category::Load) ? "load" : "runtime";
	(*m_out) << "[lua " << cat << " " << e.status << "] " << e.message << '\n';
}

} // namespace Lua
