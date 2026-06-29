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
	 * Return value handling:
	 *   - Primitives are pushed as Lua values.
	 *   - Class-type returns (by value, reference, or pointer) are wrapped as
	 *     a fresh userdata holding a *copy* of the returned object. Aliasing
	 *     is not preserved: if the method returns U& or U* into internal
	 *     state, mutations on the Lua side will not propagate back to the C++
	 *     owner. Pointer nullptr becomes Lua nil. To expose live state, bind
	 *     explicit getters/setters instead of returning U& / U*.
	 *
	 * @throws std::runtime_error if T's metatable has not been registered.
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
	 * @throws std::runtime_error if T's metatable has not been registered.
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
	 * @throws std::runtime_error if @p tableName is not a global table.
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
	 * @throws std::runtime_error if @p tableName is not a global table.
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

// This header only declares Bind. The template implementations live in the
// public <luacpp/Bind.hpp>, which includes a complete State first and then
// defines these members. Translation units that register classes include
// <luacpp/Bind.hpp>; State.hpp alone is enough to *hold* the binding facade
// (its forwarders are dependent and need only this declaration).

#endif // LUACPP_DETAIL_BIND_HPP
