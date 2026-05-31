#ifndef LUACPP_WARNINGHANDLING_HPP
#define LUACPP_WARNINGHANDLING_HPP

#include <functional>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Lua {

// ---------------------------------------------------------------------------
// WarningLogger — sink for Lua's warning system (lua_setwarnf).
//
// Per-State, swappable. Default-constructed States install a
// StreamWarningLogger to std::cerr, mirroring the loud-by-default policy of
// ErrorLogger. Pass nullptr to State::setWarningLogger to silence warnings
// entirely. Scripts can still toggle reporting at runtime via the standard
// control directives `warn("@off")` / `warn("@on")` even after a logger is
// installed.
//
// Multi-piece messages: Lua may emit a warning in fragments (tocont = 1
// on every fragment except the last). State assembles the fragments and
// surfaces a single complete message per warning to the logger.
//
// Control messages (those starting with '@') are handled internally and
// never forwarded — they are the warning system's own protocol, not
// content a logger would want to record.
//
// Warnings live in their own header (not ErrorHandling.hpp) because Lua's
// warning subsystem is API-distinct from the error path: warnings come
// from lua_setwarnf, never abort execution, and carry only plain string
// content — they share nothing with LuaError's category/status/prefix
// machinery.
// ---------------------------------------------------------------------------

class WarningLogger {
public:
	virtual ~WarningLogger() = default;
	virtual void log(const std::string& message) = 0;
};

/**
 * @brief Writes one line per warning to an std::ostream. Default-constructs
 *        to std::cerr; supply any other ostream for files, in-memory buffers,
 *        or platform-specific sinks. The referenced stream must outlive the
 *        logger.
 */
class StreamWarningLogger : public WarningLogger {
public:
	StreamWarningLogger();                              // → std::cerr
	explicit StreamWarningLogger(std::ostream& out) noexcept;
	void log(const std::string& message) override;
private:
	std::ostream* m_out;
};

/**
 * @brief Accumulates every warning in an in-memory vector. Pull the contents
 *        via entries(); clear() resets. Mirrors MemoryLogger for errors.
 */
class MemoryWarningLogger : public WarningLogger {
public:
	void log(const std::string& message) override { m_entries.push_back(message); }
	const std::vector<std::string>& entries() const noexcept { return m_entries; }
	void clear() noexcept { m_entries.clear(); }
private:
	std::vector<std::string> m_entries;
};

/**
 * @brief Bridge to an external logging system: forwards each warning to a
 *        user-supplied std::function. Mirrors CallbackLogger for errors.
 */
class CallbackWarningLogger : public WarningLogger {
public:
	explicit CallbackWarningLogger(std::function<void(const std::string&)> callback)
	    : m_callback(std::move(callback)) {
		if (!m_callback) {
			throw std::invalid_argument("CallbackWarningLogger: callback must not be null");
		}
	}
	void log(const std::string& message) override { m_callback(message); }
private:
	std::function<void(const std::string&)> m_callback;
};

} // namespace Lua

#endif // LUACPP_WARNINGHANDLING_HPP
