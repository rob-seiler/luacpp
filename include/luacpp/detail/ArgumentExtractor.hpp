#ifndef LUACPP_DETAIL_ARGUMENT_EXTRACTOR_HPP
#define LUACPP_DETAIL_ARGUMENT_EXTRACTOR_HPP

#include "../State.hpp"
#include "../Metatable.hpp"
#include "../Basics.hpp"

#include <type_traits>

namespace Lua {
namespace detail {

template <typename T>
struct is_class_type {
	static constexpr bool value =
		std::is_class_v<std::remove_cv_t<std::remove_reference_t<T>>>;
};

template <typename Arg, typename Enable = void>
struct ArgumentExtractor {
	static Arg extract(State& state, int index) {
		return state.getArgument<Arg>(index);
	}
};

// Extraction for class-type arguments (T, T&, const T&).
//
// The body returns `*ud` — an lvalue reference to the userdata-owned object.
// What happens at the call site depends on `Arg`:
//   - `const T&` / `T&`: the reference binds directly to the userdata storage.
//     No copy, no move; the referenced object lives as long as Lua's GC keeps
//     the userdata.
//   - `T` (by value): the return value is copy-constructed from `*ud`. This
//     requires `T` to be copy-constructible — binding a method that takes a
//     non-copyable class by value will fail to compile here. Use `const T&`
//     or `T*` for non-copyable types.
//   - `T&&` (rvalue ref): would require `std::move(*ud)` to bind. Currently
//     unsupported; such a signature also fails to compile here. Rvalue-ref
//     parameters on bound methods are a deliberate gap — pass by value or
//     `const T&` instead.
//
// Pointer arguments (`T*`) take the primary template path and go through
// `Stack<T*>::get`, which does its own `checkUserData` + cast and returns the
// userdata pointer directly (no copy).
template <typename Arg>
struct ArgumentExtractor<Arg, std::enable_if_t<is_class_type<Arg>::value>> {
	static Arg extract(State& state, int index) {
		using Clean = std::remove_cv_t<std::remove_reference_t<Arg>>;
		void* ud = Basics::checkUserData(state.getState(), index,
		                                 Metatable<Clean>::metatableName());
		return *static_cast<Clean*>(ud);
	}
};

} // namespace detail
} // namespace Lua

#endif // LUACPP_DETAIL_ARGUMENT_EXTRACTOR_HPP
