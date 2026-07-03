#ifndef LUACPP_ERRORHANDLING_HPP
#define LUACPP_ERRORHANDLING_HPP

#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Lua {

/**
 * @brief A Lua stack traceback (luaL_traceback output): the
 *        "stack traceback:" header plus one line per call level.
 *        Obtained via LuaMessage::traceback().
 */
class Traceback {
public:
	struct Frame {
		std::string        raw;    ///< the line as printed by Lua, without the leading tab
		std::string        source; ///< chunk name or "[C]"; empty for pseudo-lines
		std::optional<int> line;   ///< nullopt for [C] frames and pseudo-lines
		std::string        what;   ///< "global 'name'", "main chunk", "?" ...
	};

	Traceback() = default;
	explicit Traceback(std::string text) : m_text(std::move(text)) {}

	/// The traceback block exactly as Lua produced it.
	const std::string& text() const noexcept { return m_text; }

	bool empty() const noexcept { return m_text.empty(); }

	/// Parse into one Frame per line (header excluded). Best-effort:
	/// unrecognized lines (e.g. "(...tail calls...)") survive as frames
	/// with only `raw` filled. Parses on demand — cache if iterated often.
	std::vector<Frame> asList() const;

private:
	std::string m_text;
};

/**
 * @brief Lua-side error text plus best-effort accessors for Lua's standard
 *        "<chunkname>:<line>: <text>" prefix. raw() always returns the
 *        original; source()/line() return nullopt and text() falls back
 *        to raw() when the message doesn't match (table errors, level=0
 *        error(), custom message handler). With traceback support enabled
 *        the stack is carried out-of-band: raw()/text() stay pure message,
 *        traceback() returns the block, full() combines both.
 */
class LuaMessage {
public:
	LuaMessage() = default;
	LuaMessage(std::string raw) : m_raw(std::move(raw)) {}
	LuaMessage(const char* raw) : m_raw(raw ? raw : "") {}
	LuaMessage(std::string raw, std::string traceback)
	    : m_raw(std::move(raw)), m_traceback(std::move(traceback)) {}

	const std::string& raw()   const noexcept { return m_raw; }
	bool               empty() const noexcept { return m_raw.empty(); }

	/// "<chunkname>" portion of the prefix; nullopt if no parseable prefix.
	std::optional<std::string> source() const;

	/// "<line>" portion; nullopt if no parseable prefix.
	std::optional<int> line() const;

	/// Message body without the "<src>:<line>: " prefix; raw() if no prefix.
	std::string text() const;

	/// The "stack traceback:" block; nullopt when none was captured (opt-in
	/// disabled, or a Load error — nothing ran, so there is no call stack).
	std::optional<Traceback> traceback() const;

	/// raw() plus the traceback block when present — the complete printable
	/// text. Used by LuaException::what() and StreamLogger.
	std::string full() const;

	// Substring search forwarders. Other string ops go through raw().
	std::size_t find(const std::string& s, std::size_t pos = 0) const noexcept { return m_raw.find(s, pos); }
	std::size_t find(const char* s,        std::size_t pos = 0) const          { return m_raw.find(s, pos); }
	std::size_t find(char c,               std::size_t pos = 0) const noexcept { return m_raw.find(c, pos); }

private:
	std::string m_raw;
	std::string m_traceback; // empty = none captured
};

/// Streams the full Lua message (incl. traceback). For category/status-
/// formatted output use StreamLogger, which prepends "[lua <cat> <n>] ".
std::ostream& operator<<(std::ostream& os, const LuaMessage& m);

/**
 * @brief A Lua error surfaced to C++. Category splits Load (loadstring/file
 *        failed before running) from Runtime (pcall threw / OOM / msg-handler
 *        error). Status mirrors Lua's status codes plus luacpp-detected
 *        synthetic codes (see below).
 */
struct LuaError {
	enum class Category { Load, Runtime };

	// Positive values mirror Lua's LUA_OK..LUA_ERRFILE (pinned by static_assert
	// in ErrorHandling.cpp); negative values are luacpp-side detections.
	enum class Status : int {
		// Lua status mirror (positive):
		Ok                  = 0,   // LUA_OK
		Yield               = 1,   // LUA_YIELD
		RuntimeError        = 2,   // LUA_ERRRUN
		SyntaxError         = 3,   // LUA_ERRSYNTAX
		MemoryError         = 4,   // LUA_ERRMEM
		MsgHandlerError     = 5,   // LUA_ERRERR (error in the error handler)
		FileError           = 6,   // LUA_ERRFILE

		// luacpp-side detections (negative):
		FunctionNotFound    = -1,  // executeFunction: name doesn't resolve
		RegistryKeyNotFound = -2,  // executeScript: key missing or non-function
		InvalidKey          = -3,  // Registry: key Generic carries an unsupported type
	};

	Category   category;
	Status     status;
	LuaMessage message;  ///< Lua's error string, with parsing helpers

	/// True if this error was detected by luacpp itself (Status < 0).
	bool isLuacppError() const noexcept { return static_cast<int>(status) < 0; }
};

/// Stable human-readable name for a Status value. Returns "Unknown" for
/// values outside the enum (e.g. obtained via static_cast).
const char* describe(LuaError::Status status) noexcept;

/**
 * @brief Exception thrown by ThrowHandler. Carries the original LuaError.
 */
class LuaException : public std::runtime_error {
public:
	explicit LuaException(LuaError err)
	    : std::runtime_error(err.message.full()), m_error(std::move(err)) {}

	const LuaError& error() const noexcept { return m_error; }

private:
	LuaError m_error;
};

// ErrorLogger — passive observer slot ("what happened, recorded somewhere").
// Default: StreamLogger to std::cerr; nullptr silences logging.

class ErrorLogger {
public:
	virtual ~ErrorLogger() = default;
	virtual void log(const LuaError&) = 0;
};

/**
 * @brief Writes one line per error to an std::ostream. Default-constructs to
 *        std::cerr; supply any other ostream for files, in-memory buffers,
 *        or platform-specific sinks. The referenced stream must outlive the
 *        logger.
 */
class StreamLogger : public ErrorLogger {
public:
	StreamLogger();                              // → std::cerr (impl in .cpp to keep <iostream> out)
	explicit StreamLogger(std::ostream& out) noexcept;
	void log(const LuaError&) override;
private:
	std::ostream* m_out;
};

/**
 * @brief Accumulates every LuaError in an in-memory vector. Pull the contents
 *        via entries(); clear() resets. Replaces the previous LogDecorator
 *        ergonomic.
 */
class MemoryLogger : public ErrorLogger {
public:
	void log(const LuaError& e) override { m_entries.push_back(e); }
	const std::vector<LuaError>& entries() const noexcept { return m_entries; }
	void clear() noexcept { m_entries.clear(); }
private:
	std::vector<LuaError> m_entries;
};

/**
 * @brief Bridge to an external logging system: forwards each error to a
 *        user-supplied std::function. Useful for plugging spdlog/glog/your
 *        own sink without writing a subclass.
 */
class CallbackLogger : public ErrorLogger {
public:
	explicit CallbackLogger(std::function<void(const LuaError&)> callback)
	    : m_callback(std::move(callback)) {
		if (!m_callback) {
			throw std::invalid_argument("CallbackLogger: callback must not be null");
		}
	}
	void log(const LuaError& e) override { m_callback(e); }
private:
	std::function<void(const LuaError&)> m_callback;
};

// ErrorHandler — active reaction slot ("what to do about it"). Default
// nullptr (logger still runs). ThrowHandler escalates to LuaException;
// CallbackHandler bridges to std::function. Logger fires before handler.

class ErrorHandler {
public:
	virtual ~ErrorHandler() = default;
	virtual void operator()(const LuaError&) = 0;
};

/**
 * @brief Turns every reported error into a thrown LuaException.
 *
 * Place on the handler slot to escalate failures past the call site. The
 * logger (if any) runs first, so the throw does not erase the log record.
 */
class ThrowHandler : public ErrorHandler {
public:
	[[noreturn]] void operator()(const LuaError& e) override {
		throw LuaException(e);
	}
};

/**
 * @brief Reacts to errors via a user-supplied std::function. Lets the user
 *        write arbitrary flow control (conditional throw, abort, custom
 *        exception type) without subclassing.
 */
class CallbackHandler : public ErrorHandler {
public:
	explicit CallbackHandler(std::function<void(const LuaError&)> callback)
	    : m_callback(std::move(callback)) {
		if (!m_callback) {
			throw std::invalid_argument("CallbackHandler: callback must not be null");
		}
	}
	void operator()(const LuaError& e) override { m_callback(e); }
private:
	std::function<void(const LuaError&)> m_callback;
};

// Per-VM error response: passive logger + active handler. Lives by value
// inside detail::StateContext so every wrapper around the same lua_State
// (including the transient one Lua builds for a debug hook) sees the same
// configured logging/handling.

struct ErrorPolicy {
	std::unique_ptr<ErrorLogger>  logger  = nullptr; ///< passive observer; default StreamLogger (set in StateRegistry::acquire)
	std::unique_ptr<ErrorHandler> handler = nullptr; ///< active reaction; none by default
};

} // namespace Lua

#endif // LUACPP_ERRORHANDLING_HPP
