#ifndef LUACPP_METHOD_REGISTRY_HPP
#define LUACPP_METHOD_REGISTRY_HPP

struct lua_State;

namespace Lua {

class State;

/**
 * @brief Helper class for binding C++ member functions as Lua methods
 *
 * Methods are dispatched via the metatable's __index table, so they are
 * callable from Lua with colon syntax: `obj:method(args)`.
 *
 * Prerequisite: Metatable<T>::registerMetatable(state) must be called before
 * registering methods for T.
 */
class MethodRegistry {
public:
	/**
	 * @brief Bind a C++ member function as a Lua method on T's metatable
	 * @tparam T The class whose metatable receives the method
	 * @tparam Method Non-type template parameter: pointer-to-member-function
	 * @param state The Lua state
	 * @param name Name of the method in Lua (field in __index table)
	 *
	 * Example:
	 * @code
	 * Metatable<Vector2D>::registerMetatable(lua);
	 * MethodRegistry::registerMethod<Vector2D, &Vector2D::length>(lua, "length");
	 * // In Lua: v:length()
	 * @endcode
	 */
	template <typename T, auto Method>
	static void registerMethod(State& state, const char* name);
};

} // namespace Lua

#ifdef LUACPP_STATE_HPP
#include "detail/MethodRegistryImpl.inl"
#endif

#endif // LUACPP_METHOD_REGISTRY_HPP
