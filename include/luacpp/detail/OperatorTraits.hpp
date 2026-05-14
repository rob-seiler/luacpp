#ifndef LUACPP_OPERATORTRAITS_HPP
#define LUACPP_OPERATORTRAITS_HPP

#include <type_traits>
#include <utility>
#include <string>

namespace Lua {

template <typename, typename = void>
struct has_add_operator : std::false_type { };

template <typename T>
struct has_add_operator<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_sub_operator : std::false_type { };

template <typename T>
struct has_sub_operator<T, std::void_t<decltype(std::declval<T>() - std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_mul_operator : std::false_type { };

template <typename T>
struct has_mul_operator<T, std::void_t<decltype(std::declval<T>() * std::declval<T>())>> : std::true_type { };

template <typename, typename = void>
struct has_div_operator : std::false_type { };

template <typename T>
struct has_div_operator<T, std::void_t<decltype(std::declval<T>() / std::declval<T>())>> : std::true_type { };

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
