#ifndef LUACPP_DETAIL_BIND_HPP
#define LUACPP_DETAIL_BIND_HPP

struct lua_State;

namespace Lua {

class State;

/**
 * @brief Helper class for binding C++ types to Lua via metatables
 *
 * Reads naturally as a verb-noun pair:
 *   Bind::constructor<Vec, float, float>(lua, "Vec");
 *   Bind::method<Vec, &Vec::length>(lua, "length");
 */
class Bind {
public:
	/**
	 * @brief Register a C++ constructor as a callable Lua function
	 *
	 * Creates a global callable table that constructs instances of T directly
	 * into Lua userdata. Uses __call metamethod, so `Name(args)` from Lua
	 * invokes T's constructor.
	 *
	 * @code
	 * Bind::constructor<Vector2D, float, float>(lua, "Vector");
	 * // Lua: v = Vector(3, 4)
	 * @endcode
	 */
	template <typename T, typename... Args>
	static void constructor(State& state, const char* name);

	/**
	 * @brief Bind a C++ member function as a Lua method on T's metatable
	 *
	 * Method goes into the metatable's __index table, dispatched via Lua's
	 * colon syntax. Prerequisite: Metatable<T>::registerMetatable(state).
	 *
	 * @code
	 * Bind::method<Vector2D, &Vector2D::length>(lua, "length");
	 * // Lua: v:length()
	 * @endcode
	 */
	template <typename T, auto Method>
	static void method(State& state, const char* name);

	/**
	 * @brief Bind a C++ data member as a Lua property on T's metatable
	 *
	 * Field is exposed as a read/write property: `v.name` reads it, `v.name = x`
	 * writes it. Uses __index/__newindex dispatchers; Lua errors on assignment to
	 * unknown properties.
	 *
	 * @code
	 * Bind::property<Vec, &Vec::x>(lua, "x");
	 * // Lua: print(v.x); v.x = 10
	 * @endcode
	 */
	template <typename T, auto Field>
	static void property(State& state, const char* name);

	/**
	 * @brief Attach a value as a static field on a constructor table.
	 *
	 * The table @p tableName must already exist (typically created by
	 * Bind::constructor). The value is pushed and assigned as a field on it.
	 *
	 * @code
	 * Bind::constructor<Vec, float, float>(lua, "Vec");
	 * Bind::staticField(lua, "Vec", "EPSILON", 0.001f);
	 * // Lua: print(Vec.EPSILON)
	 * @endcode
	 */
	template <typename V>
	static void staticField(State& state, const char* tableName, const char* fieldName, V value);

	/**
	 * @brief Attach a free (or static member) function as a static method on a constructor table.
	 *
	 * The function is called without a self argument; Lua arguments start at index 1.
	 * Return values are pushed via Metatable<R>::create when R is a user type,
	 * or as a Lua primitive when R is known to Stack.
	 *
	 * @code
	 * Bind::constructor<Vec, float, float>(lua, "Vec");
	 * Bind::staticFunction<&Vec::fromAngle>(lua, "Vec", "fromAngle");
	 * // Lua: v = Vec.fromAngle(3.14)
	 * @endcode
	 */
	template <auto Fn>
	static void staticFunction(State& state, const char* tableName, const char* funcName);
};

} // namespace Lua

#ifdef LUACPP_STATE_HPP
#include "BindImpl.inl"
#endif

#endif // LUACPP_DETAIL_BIND_HPP
