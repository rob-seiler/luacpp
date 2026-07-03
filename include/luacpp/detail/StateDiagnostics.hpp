#ifndef LUACPP_DETAIL_STATEDIAGNOSTICS_HPP
#define LUACPP_DETAIL_STATEDIAGNOSTICS_HPP

#include "../ErrorHandling.hpp"
#include "../WarningHandling.hpp"
#include "StateRegistry.hpp" // detail::DebugHook

#include <memory>
#include <utility>

namespace Lua {

class State;

/**
 * @brief Diagnostics facade: error logger / handler, warning sink, debug hook.
 *
 * Reached as the @c diagnostics member of a State:
 *   state.diagnostics.setLogger(std::make_unique<StreamLogger>(std::cerr));
 *   auto& log = state.diagnostics.installLogger<MemoryLogger>();
 *
 * Holds a reference to its owning State and writes through the shared per-VM
 * context. A friend of State so it can reach the context directly. Neither
 * copyable nor movable.
 */
class Diagnostics {
public:
	explicit Diagnostics(State& owner) : m_state(owner) {}

	Diagnostics(const Diagnostics&) = delete;
	Diagnostics& operator=(const Diagnostics&) = delete;
	Diagnostics(Diagnostics&&) = delete;
	Diagnostics& operator=(Diagnostics&&) = delete;

	/// Install the passive observer for LuaErrors (nullptr silences logging).
	void setLogger(std::unique_ptr<ErrorLogger> logger);

	/// Construct a logger in place, install it, return a reference for inspection.
	template <typename LoggerT, typename... Args>
	LoggerT& installLogger(Args&&... args) {
		auto logger = std::make_unique<LoggerT>(std::forward<Args>(args)...);
		LoggerT* ptr = logger.get();
		setLogger(std::move(logger));
		return *ptr;
	}

	/// Install the active reaction for LuaErrors (nullptr = logger only).
	void setErrorHandler(std::unique_ptr<ErrorHandler> handler);

	/// Construct a handler in place, install it, return a reference for inspection.
	template <typename HandlerT, typename... Args>
	HandlerT& installErrorHandler(Args&&... args) {
		auto handler = std::make_unique<HandlerT>(std::forward<Args>(args)...);
		HandlerT* ptr = handler.get();
		setErrorHandler(std::move(handler));
		return *ptr;
	}

	/// Install the sink for Lua's warning system (nullptr disables warnings).
	void setWarningLogger(std::unique_ptr<WarningLogger> logger);

	/// Construct a warning logger in place, install it, return a reference.
	template <typename LoggerT, typename... Args>
	LoggerT& installWarningLogger(Args&&... args) {
		auto logger = std::make_unique<LoggerT>(std::forward<Args>(args)...);
		LoggerT* ptr = logger.get();
		setWarningLogger(std::move(logger));
		return *ptr;
	}

	/**
	 * @brief Opt in to Lua stack tracebacks on failed protected calls.
	 *
	 * When enabled, protected calls run a luaL_traceback message handler
	 * that captures the stack alongside the error: LuaMessage::traceback()
	 * returns it, full() combines it with the message. raw()/text() stay
	 * pure message, identical to the disabled path. Per-VM (shared context,
	 * like the error policy), default off. Load errors never carry a
	 * traceback (nothing ran yet); Lua skips the handler on memory errors
	 * (LUA_ERRMEM), and a call degrades to a plain pcall if the stack
	 * cannot grow by the handler's slots.
	 */
	void setTracebackEnabled(bool enabled) noexcept;

	/// Whether traceback support is currently enabled for this VM.
	bool tracebackEnabled() const noexcept;

	/**
	 * @brief Register a debug hook called on the selected VM events.
	 *
	 * @throws std::logic_error if called on a borrowed State (one constructed
	 *         from an existing lua_State*) — only the owning State may install
	 *         per-VM hooks.
	 */
	void registerDebugHook(detail::DebugHook hook, int mask, int count = 0);

private:
	State& m_state;
};

} // namespace Lua

#endif // LUACPP_DETAIL_STATEDIAGNOSTICS_HPP
