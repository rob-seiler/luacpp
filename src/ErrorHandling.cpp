#include <ErrorHandling.hpp>

#include <charconv>
#include <iostream>
#include <ostream>
#include <string_view>
#include <system_error>

namespace Lua {

namespace {

// Lua's standard error format is "<source>:<line>: <text>". The space after
// the line-colon is the reliable boundary — text can contain anything,
// including more ": ", and source can contain ":" (Windows paths). We find
// the FIRST ": " from the left, then walk back to the previous ":" — what's
// between must be the line number for the parse to succeed.
struct Parsed {
	std::string_view source;
	int              line;
	std::string_view text;
};

std::optional<Parsed> parsePrefix(const std::string& raw) {
	const auto sep = raw.find(": ");
	if (sep == std::string::npos || sep == 0) return std::nullopt;

	const auto lineColon = raw.rfind(':', sep - 1);
	if (lineColon == std::string::npos) return std::nullopt;

	const char* lineBegin = raw.data() + lineColon + 1;
	const char* lineEnd   = raw.data() + sep;
	if (lineBegin == lineEnd) return std::nullopt;

	int lineNum = 0;
	const auto r = std::from_chars(lineBegin, lineEnd, lineNum);
	if (r.ec != std::errc{} || r.ptr != lineEnd) return std::nullopt;

	return Parsed{
	    std::string_view(raw.data(), lineColon),
	    lineNum,
	    std::string_view(raw.data() + sep + 2, raw.size() - sep - 2),
	};
}

} // namespace

std::optional<std::string> LuaMessage::source() const {
	auto p = parsePrefix(m_raw);
	if (!p) return std::nullopt;
	return std::string(p->source);
}

std::optional<int> LuaMessage::line() const {
	auto p = parsePrefix(m_raw);
	if (!p) return std::nullopt;
	return p->line;
}

std::string LuaMessage::text() const {
	auto p = parsePrefix(m_raw);
	if (!p) return m_raw;
	return std::string(p->text);
}

std::ostream& operator<<(std::ostream& os, const LuaMessage& m) {
	return os << m.raw();
}

StreamLogger::StreamLogger() : m_out(&std::cerr) {}

StreamLogger::StreamLogger(std::ostream& out) noexcept : m_out(&out) {}

void StreamLogger::log(const LuaError& e) {
	const char* cat = (e.category == LuaError::Category::Load) ? "load" : "runtime";
	(*m_out) << "[lua " << cat << ' ';
	if (e.status == LuaError::SyntheticStatus) {
		(*m_out) << "synthetic";
	} else {
		(*m_out) << e.status;
	}
	(*m_out) << "] " << e.message.raw() << '\n';
}

} // namespace Lua
