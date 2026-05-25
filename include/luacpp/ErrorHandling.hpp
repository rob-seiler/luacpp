#ifndef LUACPP_ERRORHANDLING_HPP
#define LUACPP_ERRORHANDLING_HPP

#include <functional>
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
 * @brief Exception thrown by ThrowDecorator. Carries the original LuaError.
 */
class LuaException : public std::runtime_error {
public:
	explicit LuaException(LuaError err)
	    : std::runtime_error(err.message), m_error(std::move(err)) {}

	const LuaError& error() const noexcept { return m_error; }

private:
	LuaError m_error;
};

/**
 * @brief Strategy interface: invoked by the State whenever a LuaError occurs.
 *
 * Decorator composition is the intended usage model — see the decorators
 * below. A bare ErrorHandler implementation (or a subclass) can be used
 * directly as a leaf.
 *
 * Convention "outer-first": pass-through decorators (Log, Capture, Filter,
 * Callback) run their own logic and then delegate to the wrapped handler.
 * Terminal decorators (Throw) delegate first and then perform their terminal
 * action. As a result, the outermost decorator's logic always runs first
 * irrespective of the composition order — composition is commutative for
 * the user's mental model.
 */
class ErrorHandler {
public:
	virtual ~ErrorHandler() = default;
	virtual void operator()(const LuaError&) = 0;
};

/**
 * @brief Leaf handler that does nothing. Marks the end of a decorator chain.
 */
class NullHandler : public ErrorHandler {
public:
	void operator()(const LuaError&) override {}
};

/**
 * @brief Base for decorators that wrap a nested handler.
 *
 * Owns the nested handler via unique_ptr. Construction enforces a non-null
 * inner handler — a chain must terminate in a NullHandler (or other leaf),
 * not in nullptr.
 */
class ErrorHandlerDecorator : public ErrorHandler {
public:
	explicit ErrorHandlerDecorator(std::unique_ptr<ErrorHandler> next)
	    : m_next(std::move(next)) {
		if (!m_next) {
			throw std::invalid_argument(
			    "ErrorHandlerDecorator: inner handler must not be null "
			    "(terminate chains with NullHandler)");
		}
	}

protected:
	ErrorHandler& next() { return *m_next; }

private:
	std::unique_ptr<ErrorHandler> m_next;
};

/**
 * @brief Append every error to an in-memory log, then delegate.
 *
 * Typical default-handler position: lets callers retrieve everything that
 * happened during a script run via log().
 */
class LogDecorator : public ErrorHandlerDecorator {
public:
	LogDecorator() : ErrorHandlerDecorator(std::make_unique<NullHandler>()) {}
	using ErrorHandlerDecorator::ErrorHandlerDecorator;

	void operator()(const LuaError& e) override {
		m_entries.push_back(e);
		next()(e);
	}

	const std::vector<LuaError>& log() const noexcept { return m_entries; }
	void clear() noexcept { m_entries.clear(); }

private:
	std::vector<LuaError> m_entries;
};

/**
 * @brief Remember the most recent error and delegate.
 *
 * Pull-style counterpart to LogDecorator: keeps a single slot rather than
 * an unbounded history.
 */
class CaptureDecorator : public ErrorHandlerDecorator {
public:
	CaptureDecorator() : ErrorHandlerDecorator(std::make_unique<NullHandler>()) {}
	using ErrorHandlerDecorator::ErrorHandlerDecorator;

	void operator()(const LuaError& e) override {
		m_last = e;
		next()(e);
	}

	const std::optional<LuaError>& lastError() const noexcept { return m_last; }
	void reset() noexcept { m_last.reset(); }

private:
	std::optional<LuaError> m_last;
};

/**
 * @brief Delegate only when the predicate returns true for the error.
 *
 * Compose with the other decorators to apply per-category or per-status
 * behavior — e.g. wrap a ThrowDecorator behind a FilterDecorator that
 * matches only LUA_ERRSYNTAX.
 */
class FilterDecorator : public ErrorHandlerDecorator {
public:
	FilterDecorator(std::function<bool(const LuaError&)> predicate,
	                std::unique_ptr<ErrorHandler> next)
	    : ErrorHandlerDecorator(std::move(next)),
	      m_predicate(std::move(predicate)) {
		if (!m_predicate) {
			throw std::invalid_argument(
			    "FilterDecorator: predicate must not be null");
		}
	}

	void operator()(const LuaError& e) override {
		if (m_predicate(e)) next()(e);
	}

private:
	std::function<bool(const LuaError&)> m_predicate;
};

/**
 * @brief Invoke a user callback, then delegate. The callback runs first so
 *        any logging done by inner decorators reflects post-callback state.
 */
class CallbackDecorator : public ErrorHandlerDecorator {
public:
	CallbackDecorator(std::function<void(const LuaError&)> callback,
	                  std::unique_ptr<ErrorHandler> next)
	    : ErrorHandlerDecorator(std::move(next)),
	      m_callback(std::move(callback)) {
		if (!m_callback) {
			throw std::invalid_argument(
			    "CallbackDecorator: callback must not be null");
		}
	}

	void operator()(const LuaError& e) override {
		m_callback(e);
		next()(e);
	}

private:
	std::function<void(const LuaError&)> m_callback;
};

/**
 * @brief Terminal decorator: delegates to inner handler first (so logs and
 *        captures along the chain still happen), then throws LuaException.
 *
 * Place at the OUTERMOST position when you want every error to escape via
 * C++ exception. Place behind a FilterDecorator to throw only on selected
 * errors.
 */
class ThrowDecorator : public ErrorHandlerDecorator {
public:
	ThrowDecorator() : ErrorHandlerDecorator(std::make_unique<NullHandler>()) {}
	using ErrorHandlerDecorator::ErrorHandlerDecorator;

	void operator()(const LuaError& e) override {
		next()(e);
		throw LuaException(e);
	}
};

} // namespace Lua

#endif // LUACPP_ERRORHANDLING_HPP
