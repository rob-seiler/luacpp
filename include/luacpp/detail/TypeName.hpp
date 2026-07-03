#ifndef LUACPP_DETAIL_TYPENAME_HPP
#define LUACPP_DETAIL_TYPENAME_HPP

// Compile-time, compiler-independent type names for metatable registry keys.
//
// The default metatable name used to be typeid(T).name(), which is not
// portable: MSVC yields "struct myns::Grid", GCC/Clang a mangled "N4myns4"
// string. A host and a plugin built with different compilers around the same
// lua_State would then disagree on the registry key and fail to recognize
// each other's bound types. Parsing the compiler's function signature macro
// at compile time (the entt/ctti technique) yields the same qualified name
// ("myns::Grid") on MSVC, GCC and Clang.
//
// Always-defined capability macro in the spirit of Config.hpp: 0 only on
// exotic compilers, where the old typeid behavior is kept (per-compiler
// stable) instead of hard-failing the C++17 baseline promise.
#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#	define LUACPP_HAS_STABLE_TYPENAME 1
#else
#	define LUACPP_HAS_STABLE_TYPENAME 0
#endif

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>
#if !LUACPP_HAS_STABLE_TYPENAME
#	include <string>
#	include <typeinfo>
#endif

namespace Lua {
namespace detail {

// Guards the shared Lua registry namespace against collisions with metatables
// users create via State::createMetaTable: with readable default names a
// clash becomes plausible ("Vec"), with the prefix it is not ("luacpp.Vec").
// A dot rather than a colon, so the default tostring output reads
// "luacpp.Vec: 0x..." instead of a double-colon "luacpp:Vec: 0x...".
inline constexpr std::string_view MetatableNamePrefix = "luacpp.";

#if LUACPP_HAS_STABLE_TYPENAME

template <typename T>
constexpr std::string_view rawSignature() noexcept {
	// __clang__ first: clang-cl also defines _MSC_VER but produces the
	// clang-shaped "[T = ...]" signature, not MSVC's.
#if defined(__clang__) || defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#else
	return __FUNCSIG__;
#endif
}

// The type name is cut out of the signature purely positionally, with
// `double` as the probe (spelled identically, and exactly once, in all three
// compilers' signature shapes). This sidesteps every per-compiler quirk —
// MSVC's angle brackets in the return type, GCC's trailing alias expansion,
// calling-convention noise — because those are constant across instantiations.
constexpr std::size_t signaturePrefixLength() noexcept {
	return rawSignature<double>().find("double");
}

constexpr std::size_t signatureSuffixLength() noexcept {
	return rawSignature<double>().size() - signaturePrefixLength()
	     - std::string_view("double").size();
}

template <typename T>
constexpr std::string_view typeName() noexcept {
	std::string_view name = rawSignature<T>();
	name.remove_prefix(signaturePrefixLength());
	name.remove_suffix(signatureSuffixLength());
	// MSVC prints elaborated type specifiers ("struct myns::Grid"); strip the
	// leading keyword so all compilers agree. Only at the start — keywords
	// inside template arguments stay, template names are per-compiler only.
	constexpr std::string_view keywords[] = {
	    "enum class ", "enum struct ", "enum ", "class ", "struct "};
	for (std::string_view kw : keywords) {
		if (name.size() > kw.size() && name.compare(0, kw.size(), kw) == 0) {
			name.remove_prefix(kw.size());
			break;
		}
	}
	return name;
}

// Tripwire: if a future compiler ships an unexpected signature shape, fail
// loudly at compile time instead of silently corrupting registry keys.
static_assert(typeName<int>() == std::string_view("int"),
              "luacpp: type-name extraction broke on this compiler");

// The Lua C API needs a NUL-terminated const char*, so the prefixed name is
// materialized into per-type static storage at compile time.
template <typename T>
class MetatableNameStorage {
	static constexpr std::string_view name = typeName<T>();

	template <std::size_t... P, std::size_t... N>
	static constexpr std::array<char, sizeof...(P) + sizeof...(N) + 1>
	build(std::index_sequence<P...>, std::index_sequence<N...>) noexcept {
		return {{MetatableNamePrefix[P]..., name[N]..., '\0'}};
	}

public:
	static constexpr auto value =
	    build(std::make_index_sequence<MetatableNamePrefix.size()>{},
	          std::make_index_sequence<name.size()>{});
};

/// Default metatable registry key for T: "luacpp." + qualified type name,
/// identical across MSVC/GCC/Clang for named, non-template types.
template <typename T>
constexpr const char* metatableNameFor() noexcept {
	return MetatableNameStorage<T>::value.data();
}

#else // !LUACPP_HAS_STABLE_TYPENAME

// Exotic compiler: keep the old typeid behavior (stable per compiler, not
// across compilers), prefixed the same way so observable behavior is uniform.
template <typename T>
const char* metatableNameFor() {
	static const std::string name =
	    std::string(MetatableNamePrefix) + typeid(T).name();
	return name.c_str();
}

#endif // LUACPP_HAS_STABLE_TYPENAME

} // namespace detail
} // namespace Lua

#endif // LUACPP_DETAIL_TYPENAME_HPP
