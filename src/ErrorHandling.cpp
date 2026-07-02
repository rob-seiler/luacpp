#include <ErrorHandling.hpp>

#include <lua/lua.hpp>

#include <charconv>
#include <iostream>
#include <ostream>
#include <string_view>
#include <system_error>

namespace Lua {

// Pin our mirror values to Lua's actual status codes. If Lua ever renumbers,
// these will fail at compile time — flagging the drift instead of silently
// misclassifying errors at runtime.
static_assert(static_cast<int>(LuaError::Status::Ok)              == LUA_OK,        "LuaError::Status::Ok out of sync");
static_assert(static_cast<int>(LuaError::Status::Yield)           == LUA_YIELD,     "LuaError::Status::Yield out of sync");
static_assert(static_cast<int>(LuaError::Status::RuntimeError)    == LUA_ERRRUN,    "LuaError::Status::RuntimeError out of sync");
static_assert(static_cast<int>(LuaError::Status::SyntaxError)     == LUA_ERRSYNTAX, "LuaError::Status::SyntaxError out of sync");
static_assert(static_cast<int>(LuaError::Status::MemoryError)     == LUA_ERRMEM,    "LuaError::Status::MemoryError out of sync");
static_assert(static_cast<int>(LuaError::Status::MsgHandlerError) == LUA_ERRERR,    "LuaError::Status::MsgHandlerError out of sync");
static_assert(static_cast<int>(LuaError::Status::FileError)       == LUA_ERRFILE,   "LuaError::Status::FileError out of sync");

const char* describe(LuaError::Status status) noexcept {
	switch (status) {
		case LuaError::Status::Ok:                  return "Ok";
		case LuaError::Status::Yield:               return "Yield";
		case LuaError::Status::RuntimeError:        return "RuntimeError";
		case LuaError::Status::SyntaxError:         return "SyntaxError";
		case LuaError::Status::MemoryError:         return "MemoryError";
		case LuaError::Status::MsgHandlerError:     return "MsgHandlerError";
		case LuaError::Status::FileError:           return "FileError";
		case LuaError::Status::FunctionNotFound:    return "FunctionNotFound";
		case LuaError::Status::RegistryKeyNotFound: return "RegistryKeyNotFound";
		case LuaError::Status::InvalidKey:          return "InvalidKey";
	}
	return "Unknown";
}

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

std::optional<Parsed> parsePrefix(std::string_view raw) {
	const auto sep = raw.find(": ");
	if (sep == std::string_view::npos || sep == 0) return std::nullopt;

	const auto lineColon = raw.rfind(':', sep - 1);
	if (lineColon == std::string_view::npos) return std::nullopt;

	const char* lineBegin = raw.data() + lineColon + 1;
	const char* lineEnd   = raw.data() + sep;
	if (lineBegin == lineEnd) return std::nullopt;

	// std::from_chars accepts a leading '-'; Lua line numbers are always ≥ 1.
	// Reject signs up front so "src:-5: msg" doesn't parse as line = -5.
	if (*lineBegin == '-' || *lineBegin == '+') return std::nullopt;

	int lineNum = 0;
	const auto r = std::from_chars(lineBegin, lineEnd, lineNum);
	if (r.ec != std::errc{} || r.ptr != lineEnd || lineNum < 1) return std::nullopt;

	return Parsed{
	    std::string_view(raw.data(), lineColon),
	    lineNum,
	    std::string_view(raw.data() + sep + 2, raw.size() - sep - 2),
	};
}

// Split at the FIRST occurrence so error text containing the phrase later
// on cannot shift the split.
constexpr const char* tracebackMarker = "\nstack traceback:";

// Message portion before any traceback block.
std::string_view messageBody(const std::string& raw) {
	const auto pos = raw.find(tracebackMarker);
	if (pos == std::string::npos) return raw;
	return std::string_view(raw.data(), pos);
}

// "<source>[:<line>]" → Frame source/line. The LAST colon separates the
// line so drive colons ("d:\foo.lua:12") don't confuse the split.
void parseFrameLocation(std::string_view loc, Traceback::Frame& frame) {
	const auto lineColon = loc.rfind(':');
	if (lineColon != std::string_view::npos) {
		const char* lineBegin = loc.data() + lineColon + 1;
		const char* lineEnd   = loc.data() + loc.size();
		int lineNum = 0;
		const auto r = std::from_chars(lineBegin, lineEnd, lineNum);
		if (r.ec == std::errc{} && r.ptr == lineEnd && lineNum >= 1) {
			frame.source = std::string(loc.substr(0, lineColon));
			frame.line   = lineNum;
			return;
		}
	}
	frame.source = std::string(loc); // no ":<line>" suffix (currentline <= 0, or "[C]")
}

} // namespace

std::vector<Traceback::Frame> Traceback::asList() const {
	std::vector<Frame> frames;

	std::string_view rest = m_text;
	// The "stack traceback:" header carries no frame information.
	if (const auto firstBreak = rest.find('\n'); firstBreak != std::string_view::npos) {
		rest = rest.substr(firstBreak + 1);
	} else {
		return frames;
	}

	while (!rest.empty()) {
		auto lineEnd = rest.find('\n');
		std::string_view line = rest.substr(0, lineEnd);
		rest = (lineEnd == std::string_view::npos) ? std::string_view{} : rest.substr(lineEnd + 1);

		while (!line.empty() && (line.front() == '\t' || line.front() == ' ')) {
			line.remove_prefix(1);
		}
		if (line.empty()) continue;

		Frame frame;
		frame.raw = std::string(line);

		// Frame shape is "<location>: in <what>"; anything else (tail-call
		// and skip markers) stays raw-only so no line is ever lost.
		if (const auto sep = line.find(": in "); sep != std::string_view::npos) {
			parseFrameLocation(line.substr(0, sep), frame);
			frame.what = std::string(line.substr(sep + 5));
		}
		frames.push_back(std::move(frame));
	}
	return frames;
}

std::optional<std::string> LuaMessage::source() const {
	auto p = parsePrefix(messageBody(m_raw));
	if (!p) return std::nullopt;
	return std::string(p->source);
}

std::optional<int> LuaMessage::line() const {
	auto p = parsePrefix(messageBody(m_raw));
	if (!p) return std::nullopt;
	return p->line;
}

std::string LuaMessage::text() const {
	const auto body = messageBody(m_raw);
	auto p = parsePrefix(body);
	if (!p) return std::string(body);
	return std::string(p->text);
}

std::optional<Traceback> LuaMessage::traceback() const {
	const auto pos = m_raw.find(tracebackMarker);
	if (pos == std::string::npos) return std::nullopt;
	return Traceback(m_raw.substr(pos + 1)); // +1: drop the separating newline
}

std::ostream& operator<<(std::ostream& os, const LuaMessage& m) {
	return os << m.raw();
}

StreamLogger::StreamLogger() : m_out(&std::cerr) {}

StreamLogger::StreamLogger(std::ostream& out) noexcept : m_out(&out) {}

void StreamLogger::log(const LuaError& e) {
	const char* origin = e.isLuacppError() ? "luacpp" : "lua";
	const char* cat    = (e.category == LuaError::Category::Load) ? "load" : "runtime";
	(*m_out) << '[' << origin << ' ' << cat << ' ' << describe(e.status) << "] "
	         << e.message.raw() << '\n';
}

} // namespace Lua
