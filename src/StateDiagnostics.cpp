#include <State.hpp>
#include <lua/lua.hpp>

namespace Lua {

void Diagnostics::setLogger(std::unique_ptr<ErrorLogger> logger) {
	m_state.m_context->policy.logger = std::move(logger);
}

void Diagnostics::setErrorHandler(std::unique_ptr<ErrorHandler> handler) {
	m_state.m_context->policy.handler = std::move(handler);
}

void Diagnostics::setWarningLogger(std::unique_ptr<WarningLogger> logger) {
	auto& w = m_state.m_context->warning;
	w.logger = std::move(logger);
	w.buffer.clear();
	w.inProgress = false;
	w.currentIsSingle = false;
	if (w.logger) {
		// Installing a logger is the opt-in that enables warnings — Lua
		// starts the system disabled. Scripts can still flip via @off.
		w.enabled = true;
		lua_setwarnf(m_state.getState(), &detail::warningTrampoline, &w);
	} else {
		w.enabled = false;
		lua_setwarnf(m_state.getState(), nullptr, nullptr);
	}
}

void Diagnostics::setTracebackEnabled(bool enabled) noexcept {
	m_state.m_context->tracebackEnabled = enabled;
}

bool Diagnostics::tracebackEnabled() const noexcept {
	return m_state.m_context->tracebackEnabled;
}

void Diagnostics::registerDebugHook(detail::DebugHook hook, int mask, int count) {
	// The hook plumbing (owner check, per-VM hook slot, and the borrowed-view
	// State the trampoline hands to the callback) lives on State — see
	// State::installDebugHook.
	m_state.installDebugHook(std::move(hook), mask, count);
}

} // namespace Lua
