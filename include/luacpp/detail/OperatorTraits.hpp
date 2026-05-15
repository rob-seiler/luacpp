#ifndef LUACPP_OPERATORTRAITS_HPP
#define LUACPP_OPERATORTRAITS_HPP

#include <type_traits>
#include <utility>
#include <string>

namespace Lua {

// Two-type operator traits: detect whether T op U is well-formed.

template <typename T, typename U, typename = void>
struct has_add : std::false_type { };

template <typename T, typename U>
struct has_add<T, U, std::void_t<decltype(std::declval<T>() + std::declval<U>())>> : std::true_type { };

template <typename T, typename U, typename = void>
struct has_sub : std::false_type { };

template <typename T, typename U>
struct has_sub<T, U, std::void_t<decltype(std::declval<T>() - std::declval<U>())>> : std::true_type { };

template <typename T, typename U, typename = void>
struct has_mul : std::false_type { };

template <typename T, typename U>
struct has_mul<T, U, std::void_t<decltype(std::declval<T>() * std::declval<U>())>> : std::true_type { };

template <typename T, typename U, typename = void>
struct has_div : std::false_type { };

template <typename T, typename U>
struct has_div<T, U, std::void_t<decltype(std::declval<T>() / std::declval<U>())>> : std::true_type { };

// Same-type aliases retained for the existing call sites in Metatable.hpp.
template <typename T> using has_add_operator = has_add<T, T>;
template <typename T> using has_sub_operator = has_sub<T, T>;
template <typename T> using has_mul_operator = has_mul<T, T>;
template <typename T> using has_div_operator = has_div<T, T>;

template <typename, typename = void>
struct has_unary_minus_operator : std::false_type { };

template <typename T>
struct has_unary_minus_operator<T, std::void_t<decltype(-std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_eq_operator : std::false_type { };

template <typename T>
struct has_eq_operator<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_lt_operator : std::false_type { };

template <typename T>
struct has_lt_operator<T, std::void_t<decltype(std::declval<T>() < std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_le_operator : std::false_type { };

template <typename T>
struct has_le_operator<T, std::void_t<decltype(std::declval<T>() <= std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_to_string : std::false_type { };

template <typename T>
struct has_to_string<T, std::void_t<decltype(std::string(std::declval<const T&>().toString()))>> : std::true_type { };

} // namespace Lua

#endif // LUACPP_OPERATORTRAITS_HPP
