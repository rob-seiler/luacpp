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

	Category    category;
	int         status;   ///< raw Lua status code (LUA_ERRSYNTAX etc.)
	std::string message;  ///< message Lua placed on the stack
	std::string source;   ///< chunk name / file path, when known
};

/**
 * @brief Exception thrown by ThrowHandler. Carries the original LuaError.
 */
class LuaException : public std::runtime_error {
public:
	explicit LuaException(LuaError err)
	    : std::runtime_error(err.message), m_error(std::move(err)) {}

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

} // namespace Lua

#endif // LUACPP_ERRORHANDLING_HPP
