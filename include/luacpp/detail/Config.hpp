#ifndef LUACPP_DETAIL_CONFIG_HPP
#define LUACPP_DETAIL_CONFIG_HPP

// Capability detection for the optional, additively-gated modern-C++ surface.
//
// Policy: C++17 is luacpp's mandatory baseline and never moves. Newer
// language/library features are NEVER required to build or use the library.
// They light up automatically when the *consumer's* compile mode provides
// them — detected here via standard feature-test macros, not via a manual
// opt-in flag the user could set inconsistently with their actual toolchain.
//
// "detect, don't declare": nothing here asks the consumer to switch a feature
// on. The only user-facing knob is the negative override LUACPP_NO_MODERN,
// which force-disables every gated path regardless of detection — an escape
// hatch for a toolchain whose stdlib advertises a feature but ships it broken.
// Define it before including any luacpp header.
//
// Each capability is exposed as an always-defined LUACPP_HAS_* macro (1/0) so
// consumer code can branch on it with #if without worrying about whether the
// macro exists.

#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>   // canonical home of the __cpp_lib_* macros (C++20+)
#  endif
#endif

// std::span — C++20 (P0122, __cpp_lib_span). Gates the span-based overloads of
// the array-argument call entry points in State, which let callers pass a
// contiguous range instead of a (pointer, length) pair.
#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L && !defined(LUACPP_NO_MODERN)
#  define LUACPP_HAS_SPAN 1
#else
#  define LUACPP_HAS_SPAN 0
#endif

#endif // LUACPP_DETAIL_CONFIG_HPP
