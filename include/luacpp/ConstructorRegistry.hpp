#ifndef LUACPP_CONSTRUCTOR_REGISTRY_HPP
#define LUACPP_CONSTRUCTOR_REGISTRY_HPP

struct lua_State;

namespace Lua {

class State;

/**
 * @brief Helper class for registering C++ constructors as callable Lua functions
 *
 * This class provides a clean interface to make C++ types constructible from Lua
 * using intuitive syntax like `Vector(x, y)` instead of factory functions.
 */
class ConstructorRegistry {
public:
	/**
	 * @brief Register a constructor for a C++ type
	 * @tparam T The type to register a constructor for
	 * @tparam Args The argument types for the constructor
	 * @param state The Lua state to register in
	 * @param name The name of the constructor function in Lua (e.g., "Vector")
	 *
	 * Creates a global callable table in Lua that constructs instances of T.
	 * The table uses the __call metamethod to intercept calls like `Vector(3, 4)`.
	 *
	 * Example:
	 * @code
	 * ConstructorRegistry::registerConstructor<Vector2D, float, float>(lua, "Vector");
	 * // In Lua: v = Vector(3, 4)
	 * @endcode
	 */
	template <typename T, typename... Args>
	static void registerConstructor(State& state, const char* name);
};

} // namespace Lua

// Include template implementation when State is fully defined
#ifdef LUACPP_STATE_HPP
#include "detail/ConstructorRegistryImpl.inl"
#endif

#endif // LUACPP_CONSTRUCTOR_REGISTRY_HPP
