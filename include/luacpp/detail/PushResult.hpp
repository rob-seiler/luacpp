#ifndef LUACPP_DETAIL_PUSH_RESULT_HPP
#define LUACPP_DETAIL_PUSH_RESULT_HPP

// Smart push: dispatches a C++ value onto the Lua stack via Stack<T> for
// primitives or via Metatable<T>::create for user-defined class types.
//
// Lives in its own header because two equally-weighted consumers depend on
// it — Metatable.hpp's auto-generated operator wrappers, and BindImpl.inl's
// methodWrapper / freeFunctionWrapper / staticField. Keeping the definition
// here breaks the State.hpp <-> Metatable.hpp include cycle that would
// otherwise hide it from BindImpl.inl at parse time (visible in C++20
// module builds where MSVC's GMF is stricter than the classic header path).
//
// Internal header: include via <luacpp/State.hpp> or <luacpp/Metatable.hpp>,
// not directly — the include order is brittle on its own.

#include "../State.hpp"
#include "../Basics.hpp"
#include "../Type.hpp"

#include <type_traits>
#include <utility>

namespace Lua {

// Forward declaration; full definition in <luacpp/Metatable.hpp>. The call to
// Metatable<T>::create below is dependent on R, so the lookup happens at
// template instantiation — by which point any user pushing a class type into
// Lua must already have included Metatable.hpp.
template <typename T>
struct Metatable;

namespace detail {

/**
 * @brief Push a C++ value onto the Lua stack.
 *
 * Dispatch:
 *   - Primitive (number, bool, string): pushed directly.
 *   - Class type by value / rvalue: wrapped as userdata via
 *     Metatable<Clean>::create, moving the value into the userdata.
 *   - Class type by reference (T&, const T&): copied into a freshly
 *     constructed userdata. Aliasing is NOT preserved: the Lua-side userdata
 *     is an independent copy of the referenced object, and mutations on
 *     either side do not propagate to the other. Forwarding ensures the
 *     source is copied, never moved-from.
 *   - Class pointer (T*): dereferenced and copied into a userdata, same as
 *     the reference case — pointer identity is lost, mutations do not
 *     propagate back to the C++ object the pointer referred to. nullptr
 *     becomes Lua nil.
 *
 * Rationale: Lua userdata owns its storage and is collected by Lua's GC, so
 * the binding cannot safely hand out aliasing handles to C++-owned memory.
 * To expose live state, bind explicit accessor methods rather than returning
 * raw T& / T*.
 */
template <typename R>
void pushResult(State& state, R&& result) {
	using Clean = std::remove_cv_t<std::remove_reference_t<R>>;

	if constexpr (std::is_pointer_v<Clean> &&
	              std::is_class_v<std::remove_pointer_t<Clean>>) {
		using Pointee = std::remove_cv_t<std::remove_pointer_t<Clean>>;
		if (result == nullptr) {
			Basics::pushNil(state.getState());
		} else {
			Metatable<Pointee>::create(state, *result);
		}
	} else if constexpr (Basics::getTypeFor<Clean>() == Type::None) {
		Metatable<Clean>::create(state, std::forward<R>(result));
	} else {
		state.pushToStack(std::forward<R>(result));
	}
}

} // namespace detail
} // namespace Lua

#endif // LUACPP_DETAIL_PUSH_RESULT_HPP
