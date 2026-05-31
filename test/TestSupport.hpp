#pragma once

#include <luacpp/State.hpp>

#include <stdexcept>
#include <string>

namespace Lua {

// Unwrap the optional<T> returned by State::readVariable<T> or throw if the
// variable is missing / has the wrong type. Throwing fails the surrounding
// gtest loudly instead of letting a default-T value silently propagate.
// Used across the test suite (was duplicated in six TUs before extraction).
template <typename T>
T readVar(State& s, const char* name) {
	auto v = s.readVariable<T>(name);
	if (!v) {
		throw std::runtime_error(
		    std::string("readVar: '") + name + "' missing or wrong type");
	}
	return *v;
}

} // namespace Lua
