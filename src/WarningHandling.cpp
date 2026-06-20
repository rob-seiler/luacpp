#include <WarningHandling.hpp>

#include <iostream>
#include <ostream>
#include <string>

namespace Lua {

StreamWarningLogger::StreamWarningLogger() : m_out(&std::cerr) {}

StreamWarningLogger::StreamWarningLogger(std::ostream& out) noexcept : m_out(&out) {}

void StreamWarningLogger::log(const std::string& message) {
	(*m_out) << "[lua warning] " << message << '\n';
}

namespace detail {

void WarningState::handleWarning(const char* msg, int tocont) {
	// Mirror Lua's checkcontrol: a leading '@' is a control directive only
	// when the warning arrived in one piece. inProgress (not buffer.empty())
	// marks "first piece" so an empty first piece can't be confused with a
	// fresh start.
	if (!inProgress) {
		currentIsSingle = (tocont == 0);
		inProgress = true;
	}

	buffer.append(msg);
	if (tocont) return;

	// Terminal piece: swap out the buffer so the next warning starts fresh
	// even if the logger throws.
	inProgress = false;
	std::string assembled;
	assembled.swap(buffer);

	// @on / @off toggle reporting; other single-piece @-messages are
	// silently dropped (matches Lua's default warn function).
	if (currentIsSingle && !assembled.empty() && assembled.front() == '@') {
		if      (assembled == "@on")  enabled = true;
		else if (assembled == "@off") enabled = false;
		return;
	}

	if (enabled && logger) {
		logger->log(assembled);
	}
}

void warningTrampoline(void* ud, const char* msg, int tocont) {
	if (!ud || !msg) return;
	static_cast<WarningState*>(ud)->handleWarning(msg, tocont);
}

} // namespace detail
} // namespace Lua
