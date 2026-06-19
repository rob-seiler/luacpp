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
 * @brief Lua-side error text plus best-effort accessors for the standard
 *        "<chunkname>:<line>: <text>" prefix Lua's error() prepends.
 *
 * The raw string Lua placed on the stack is always available verbatim via
 * raw(). The convenience accessors source() / line() / text() parse Lua's
 * standard prefix format and return nullopt (or fall back to raw()) when
 * the message does not match — e.g. a table-thrown error, error() called
 * with level=0, or a custom message handler that rewrote the format.
 *
 * Implicit construction from std::string is allowed because LuaMessage is
 * essentially a thin wrapper that exposes parsing convenience — passing a
 * plain std::string where a LuaMessage is expected is never a misuse.
 */
class LuaMessage {
public:
	LuaMessage() = default;
	LuaMessage(std::string raw) : m_raw(std::move(raw)) {}
	LuaMessage(const char* raw) : m_raw(raw ? raw : "") {}

	const std::string& raw()   const noexcept { return m_raw; }
	bool               empty() const noexcept { return m_raw.empty(); }

	/// "<chunkname>" portion of the prefix; nullopt if no parseable prefix.
	std::optional<std::string> source() const;

	/// "<line>" portion; nullopt if no parseable prefix.
	std::optional<int> line() const;

	/// Message body without the "<src>:<line>: " prefix; raw() if no prefix.
	std::string text() const;

	// Forwarders for the common substring-search use case. Deliberately
	// narrow: only find() is delegated because that's what callers actually
	// reach for. Everything else goes through raw() to keep the API tight.
	std::size_t find(const std::string& s, std::size_t pos = 0) const noexcept { return m_raw.find(s, pos); }
	std::size_t find(const char* s,        std::size_t pos = 0) const          { return m_raw.find(s, pos); }
	std::size_t find(char c,               std::size_t pos = 0) const noexcept { return m_raw.find(c, pos); }

private:
	std::string m_raw;
};

/// Streams the raw Lua message. For category/status-formatted output use
/// StreamLogger, which wraps a LuaError and prepends "[lua <cat> <n>] ".
std::ostream& operator<<(std::ostream& os, const LuaMessage& m);

/**
 * @brief Describes a single Lua error surfaced to C++.
 *
 * The State raises a LuaError whenever a Lua API call returns a non-OK
 * status. Two categories cover the lifecycle:
 *  - Load:    luaL_loadstring / luaL_loadfile failures (LUA_ERRSYNTAX,
 *             LUA_ERRFILE). The script never started running.
 *  - Runtime: pcall failures (LUA_ERRRUN, LUA_ERRMEM, LUA_ERRERR). The
 *             script started running and threw, ran out of memory, or its
 *             own message handler errored.
 */
struct LuaError {
	enum class Category { Load, Runtime };

	/**
	 * @brief Status code: Lua's status (>=0) or a luacpp-side detection (<0).
	 *
	 * The positive values mirror Lua's LUA_OK..LUA_ERRFILE numerically — a
	 * static_assert in ErrorHandling.cpp pins this so users never need to
	 * include <lua/lua.h> to dispatch on status. Negative values are
	 * conditions luacpp detects itself before Lua sees them.
	 */
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
	    : std::runtime_error(err.message.raw()), m_error(std::move(err)) {}

	const LuaError& error() const noexcept { return m_error; }

private:
	LuaError m_error;
};

// ---------------------------------------------------------------------------
// ErrorLogger — passive observer slot ("what happened, recorded somewhere")
//
// Per-State, swappable. Default = StreamLogger to std::cerr. Pass nullptr to
// State::setLogger to silence all logging.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// ErrorHandler — active reaction slot ("what to do about it")
//
// Per-State, swappable. Default = nullptr (no reaction; the logger still
// runs). Set ThrowHandler to escalate errors as C++ exceptions, or a
// CallbackHandler for custom flow control. The logger runs before the
// handler, so even a throwing handler leaves the log entry behind.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// ErrorPolicy — the per-VM error response: passive logger + active handler.
//
// Bundled so all State wrappers around the same lua_State observe the same
// configuration. ErrorPolicy lives by value inside detail::StateContext, which
// every wrapper reaches via a raw pointer guarded by the context's intrusive
// refCount. A callback (e.g. the transient wrapper Lua builds for a debug
// hook) therefore sees the logging/handling the VM was configured with,
// instead of silently falling back to a fresh StreamLogger + null handler.
// ---------------------------------------------------------------------------

struct ErrorPolicy {
	std::unique_ptr<ErrorLogger>  logger  = nullptr; ///< passive observer; may be null
	std::unique_ptr<ErrorHandler> handler = nullptr; ///< active reaction; may be null
};

} // namespace Lua

#endif // LUACPP_ERRORHANDLING_HPP
