#ifndef LUACPP_DETAIL_STATEBINDING_HPP
#define LUACPP_DETAIL_STATEBINDING_HPP

#include "Bind.hpp" // Bind must be a complete type to name Bind::method etc.

#include <utility> // std::forward

namespace Lua {

class State;

/**
 * @brief Class-binding facade: register C++ types, methods and properties.
 *
 * Reached as the @c binding member of a State:
 *   state.binding.constructor<Vec, float, float>("Vec");
 *   state.binding.method<Vec, &Vec::length>("length");
 *
 * Thin forwarder over @ref Bind — holds a reference to its owning State and
 * carries no state of its own. Neither copyable nor movable.
 *
 * The forwarders are defined inline here even though State is only
 * forward-declared: each body merely *passes* the State& on to a Bind::
 * member template. `Bind::constructor<T,...>` is a dependent template-id, so
 * its instantiation (which needs State complete) is deferred to the caller's
 * site — where <luacpp/State.hpp> has already pulled in Bind's definitions.
 */
class Binding {
public:
	explicit Binding(State& owner) : m_state(owner) {}

	Binding(const Binding&) = delete;
	Binding& operator=(const Binding&) = delete;
	Binding(Binding&&) = delete;
	Binding& operator=(Binding&&) = delete;

	/// Bind a C++ constructor for T as a callable Lua function named @p name.
	template <typename T, typename... Args>
	void constructor(const char* name) {
		Bind::constructor<T, Args...>(m_state, name);
	}

	/// Bind a C++ member function as a Lua method on T's metatable.
	/// Prerequisite: Metatable<T>::registerMetatable(state) must have run.
	template <typename T, auto Method>
	void method(const char* name) {
		Bind::method<T, Method>(m_state, name);
	}

	/// Bind a C++ data member as a Lua property on T's metatable.
	/// Prerequisite: Metatable<T>::registerMetatable(state) must have run.
	template <typename T, auto Field>
	void property(const char* name) {
		Bind::property<T, Field>(m_state, name);
	}

	/// Attach a value as a static field on an existing constructor table.
	template <typename V>
	void staticField(const char* tableName, const char* fieldName, V value) {
		Bind::staticField(m_state, tableName, fieldName, std::forward<V>(value));
	}

	/// Attach a free function as a static method on an existing constructor table.
	template <auto Fn>
	void staticFunction(const char* tableName, const char* funcName) {
		Bind::staticFunction<Fn>(m_state, tableName, funcName);
	}

private:
	State& m_state;
};

} // namespace Lua

#endif // LUACPP_DETAIL_STATEBINDING_HPP
