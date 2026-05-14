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
