#include <WarningHandling.hpp>

#include <iostream>
#include <ostream>

namespace Lua {

StreamWarningLogger::StreamWarningLogger() : m_out(&std::cerr) {}

StreamWarningLogger::StreamWarningLogger(std::ostream& out) noexcept : m_out(&out) {}

void StreamWarningLogger::log(const std::string& message) {
	(*m_out) << "[lua warning] " << message << '\n';
}

} // namespace Lua
