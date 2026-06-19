#ifndef LUACPP_WARNINGHANDLING_HPP
#define LUACPP_WARNINGHANDLING_HPP

#include <functional>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Lua {

// WarningLogger — sink for Lua's warning system (lua_setwarnf).
//
// Opt-in: install via State::setWarningLogger to route warnings into your
// pipeline. Default = no luacpp logger; Lua's native warnfon prints to
// stderr. Passing nullptr detaches and disables warnings entirely (Lua's
// native handler can't be restored — no lua_getwarnf in the C API).
//
// Scripts can toggle reporting with `warn("@off")` / `warn("@on")`; control
// messages are handled internally and never reach the logger. Multi-piece
// warnings are assembled and surface to the logger as a single message.

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
