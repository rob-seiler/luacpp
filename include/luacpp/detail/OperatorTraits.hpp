#ifndef LUACPP_OPERATORTRAITS_HPP
#define LUACPP_OPERATORTRAITS_HPP

#include <type_traits>
#include <utility>
#include <string>

namespace Lua {

// Single-operand and toString traits used by Metatable.hpp.
// Binary/comparison operator detection now lives in detail::can_apply
// (see Metatable.hpp) parameterised on operator tag types, so the old
// per-operator has_* traits have been removed.

template <typename, typename = void>
struct has_unary_minus_operator : std::false_type { };

template <typename T>
struct has_unary_minus_operator<T, std::void_t<decltype(-std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_to_string : std::false_type { };

template <typename T>
struct has_to_string<T, std::void_t<decltype(std::string(std::declval<const T&>().toString()))>> : std::true_type { };

} // namespace Lua

#endif // LUACPP_OPERATORTRAITS_HPP
