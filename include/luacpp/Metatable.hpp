#ifndef LUACPP_METATABLE_HPP
#define LUACPP_METATABLE_HPP

#include "State.hpp"
#include "Table.hpp"
#include "Basics.hpp"
#include "detail/PushResult.hpp"

#include <type_traits>
#include <typeinfo>
#include <utility>
#include <string>
#include "detail/OperatorTraits.hpp"

namespace Lua {


namespace detail {
/**
 * @brief Safely retrieves and validates typed userdata from the Lua stack
 * @tparam T The expected userdata type
 * @param lvm The Lua state
 * @param index Stack index of the value to check
 * @return Pointer to the validated userdata of type T
 *
 * @note This function uses luaL_checkudata internally, which:
 *       - Validates the value is userdata with matching metatable
 *       - On success: returns a valid pointer (NEVER nullptr)
 *       - On failure: throws a Lua error via longjmp (NEVER returns)
 *
 * @note Therefore, nullptr checks after calling this function are unnecessary.
 *       If the function returns, the pointer is guaranteed to be valid.
 *
 * @note Error Handling: Type mismatches automatically generate Lua errors with
 *       descriptive messages. These errors propagate through Lua's error handling
 *       system and can be caught at the script level with pcall() or will be
 *       returned as error codes from State::loadAndExecuteScript().
 *
 * Example:
 * @code
 * // In C++ callback:
 * T* obj = checkUserData<T>(lvm, 1);
 * // No null check needed - if we reach here, obj is valid
 * obj->doSomething();
 *
 * // In Lua (error handling):
 * local success, err = pcall(function()
 *     myCppFunction(wrongType) -- Will error if types don't match
 * end)
 * if not success then
 *     print("Type error: " .. err)
 * end
 * @endcode
 */
template <typename T>
T* checkUserData(lua_State* lvm, int index) {
	void* ud = Basics::checkUserData(lvm, index, Metatable<T>::metatableName());
	return static_cast<T*>(ud); // Safe cast - ud is never nullptr here
}

// pushResult lives in detail/PushResult.hpp — included above so the operator
// wrappers below can use it without a Metatable<T>-self-include cycle.

// ----------------------------------------------------------------------------
// Binary arithmetic operator dispatch
// ----------------------------------------------------------------------------
// Each Op tag bundles three pieces:
//   - a SFINAE-friendly templated apply(a, b),
//   - the metatable slot to register on,
//   - the error message for "no matching operand types".
//
// registerBinaryOp<T, Op> generates a single lua_CFunction that dispatches
// among the three runtime operand-type combinations (T,T / T,double /
// double,T), enabling each branch only if the corresponding C++ expression
// is well-formed at compile time.

struct AddOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a + b) { return a + b; }
	static constexpr const char* slot = State::MetaTable::Addition;
	static constexpr const char* errorMsg = "operator+: invalid operand types";
};

struct SubOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a - b) { return a - b; }
	static constexpr const char* slot = State::MetaTable::Substraction;
	static constexpr const char* errorMsg = "operator-: invalid operand types";
};

struct MulOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a * b) { return a * b; }
	static constexpr const char* slot = State::MetaTable::Multiplication;
	static constexpr const char* errorMsg = "operator*: invalid operand types";
};

struct DivOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a / b) { return a / b; }
	static constexpr const char* slot = State::MetaTable::Division;
	static constexpr const char* errorMsg = "operator/: invalid operand types";
};

template <typename A, typename B, typename Op, typename = void>
struct can_apply : std::false_type { };

// Cross-type guard: for A != B, require that neither operand is implicitly
// convertible to the other. This blocks the case where a user type with an
// implicit constructor (e.g. `Vector(float,float)`) makes `Vector + double`
// well-formed only via `double -> Vector` conversion. Such a branch would
// silently construct a Vector from a number and also surface as a C4244
// narrowing warning inside the apply body. Users who want a real mixed-type
// operator should either mark their ctor `explicit` or specialize Metatable.
template <typename A, typename B, typename Op>
struct can_apply<A, B, Op,
                 std::void_t<decltype(Op::apply(std::declval<A>(), std::declval<B>()))>>
	: std::bool_constant<
		std::is_same_v<A, B>
		|| (!std::is_convertible_v<B, A> && !std::is_convertible_v<A, B>)
	  > { };

template <typename T, typename Op>
void registerBinaryOp(Table& mt) {
	if constexpr (can_apply<T, T, Op>::value
	           || can_apply<T, double, Op>::value
	           || can_apply<double, T, Op>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			Type t1 = Basics::getType(lvm, 1);
			Type t2 = Basics::getType(lvm, 2);
			if constexpr (can_apply<T, T, Op>::value) {
				if (t1 == Type::UserData && t2 == Type::UserData) {
					T* a = checkUserData<T>(lvm, 1);
					T* b = checkUserData<T>(lvm, 2);
					pushResult(L, Op::apply(*a, *b));
					return 1;
				}
			}
			if constexpr (can_apply<T, double, Op>::value) {
				if (t1 == Type::UserData && t2 == Type::Number) {
					T* a = checkUserData<T>(lvm, 1);
					double b = Basics::asNumber(lvm, 2);
					pushResult(L, Op::apply(*a, b));
					return 1;
				}
			}
			if constexpr (can_apply<double, T, Op>::value) {
				if (t1 == Type::Number && t2 == Type::UserData) {
					double a = Basics::asNumber(lvm, 1);
					T* b = checkUserData<T>(lvm, 2);
					pushResult(L, Op::apply(a, *b));
					return 1;
				}
			}
			return Basics::error(lvm, Op::errorMsg);
		};
		mt.setElement(Op::slot, func);
	}
}

template <typename T>
void registerUnaryMinus(Table& mt) {
	if constexpr (has_unary_minus_operator<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* obj = checkUserData<T>(lvm, 1);
			T result = -(*obj);
			Metatable<T>::create(L, result);
			return 1;
		};
		mt.setElement(State::MetaTable::UnaryMinus, func);
	}
}

// ----------------------------------------------------------------------------
// Comparison operator dispatch
// ----------------------------------------------------------------------------
// Same Op-tag pattern as the binary arithmetic ops, but the result is always
// pushed as a Lua boolean and only the (T, T) operand combination is dispatched
// (Lua's __eq/__lt/__le are only invoked when both operands share a type).

// Comparison op tags carry an `errorMsg` for symmetry with the arithmetic
// tags, even though `registerComparison` does not currently surface it:
// Lua only invokes __eq/__lt/__le when both operands share a metatable, so
// `checkUserData<T>` handles the mismatch path before any fallback would
// run. The field documents the intended message and keeps the tag shape
// uniform across the file.
struct EqOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a == b) { return a == b; }
	static constexpr const char* slot = State::MetaTable::Equal;
	static constexpr const char* errorMsg = "operator==: invalid operand types";
};

struct LtOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a < b) { return a < b; }
	static constexpr const char* slot = State::MetaTable::LessThan;
	static constexpr const char* errorMsg = "operator<: invalid operand types";
};

struct LeOp {
	template <typename A, typename B>
	static auto apply(const A& a, const B& b) -> decltype(a <= b) { return a <= b; }
	static constexpr const char* slot = State::MetaTable::LessThanOrEqual;
	static constexpr const char* errorMsg = "operator<=: invalid operand types";
};

template <typename T, typename Op>
void registerComparison(Table& mt) {
	if constexpr (can_apply<T, T, Op>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* lhs = checkUserData<T>(lvm, 1);
			T* rhs = checkUserData<T>(lvm, 2);
			L.pushToStack(static_cast<bool>(Op::apply(*lhs, *rhs)));
			return 1;
		};
		mt.setElement(Op::slot, func);
	}
}

template <typename T>
void registerToString(Table& mt) {
	if constexpr (has_to_string<T>::value) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			State L(lvm);
			T* obj = checkUserData<T>(lvm, 1);
			std::string result = obj->toString();
			L.pushToStack(result.c_str());
			return 1;
		};
		mt.setElement(State::MetaTable::Tostring, func);
	}
}

/**
 * @brief Register __gc metamethod for types with non-trivial destructors
 * @tparam T The type to register destructor for
 * @param mt The metatable to register in
 *
 * This function automatically registers a garbage collection handler that
 * properly calls the C++ destructor when Lua's garbage collector runs.
 * Only registers for types that need explicit destruction.
 */
template <typename T>
void registerGC(Table& mt) {
	// Only register __gc for types with non-trivial destructors
	if constexpr (!std::is_trivially_destructible_v<T>) {
		int (*func)(lua_State*) = [](lua_State* lvm) -> int {
			// Get the userdata - we use asUserData here because __gc is called
			// by Lua's GC, not from user code, so type is guaranteed
			T* obj = static_cast<T*>(Basics::asUserData(lvm, 1));
			if (obj != nullptr) {
				// Explicitly call destructor (placement delete)
				obj->~T();
			}
			return 0;
		};
		mt.setElement(State::MetaTable::GC, func);
	}
}

template <typename T>
void registerOperators(Table& mt) {
	registerBinaryOp<T, AddOp>(mt);
	registerBinaryOp<T, SubOp>(mt);
	registerBinaryOp<T, MulOp>(mt);
	registerBinaryOp<T, DivOp>(mt);
	registerUnaryMinus<T>(mt);
	registerComparison<T, EqOp>(mt);
	registerComparison<T, LtOp>(mt);
	registerComparison<T, LeOp>(mt);
	registerToString<T>(mt);
}

template <typename T>
void registerDefaultMetatable(State& state) {
	state.createMetaTable(Metatable<T>::metatableName(), [](Table& mt) {
		registerOperators<T>(mt);
		registerGC<T>(mt);
	});
}
} // namespace detail

/**
 * @brief Helper for binding custom C++ classes to Lua.
 *
 * Specializations can override @c metatableName() or @c registerMetatable()
 * to customize the integration. By default, common operators are registered
 * if they exist.
 */
template <typename T>
struct Metatable {
	static const char* metatableName() { return typeid(T).name(); }

	static void registerMetatable(State& state) {
		detail::registerDefaultMetatable<T>(state);
	}

	template <typename... Args>
	static T* create(State& state, Args&&... args) {
		T* obj = state.createUserData<T>(std::forward<Args>(args)...);
		state.assignMetaTable(metatableName());
		return obj;
	}
};

namespace detail {

// Specialization that satisfies the marker trait declared in Stack.hpp.
// Visible only in translation units that have included Metatable.hpp, so
// the static_assert in Stack<T*>::get fires with a precise message when
// the include is missing.
template <typename T>
struct metatable_visible<T, std::void_t<decltype(Metatable<T>::metatableName())>>
	: std::true_type {};

} // namespace detail

} // namespace Lua

#endif // LUACPP_METATABLE_HPP
