#ifndef LUACPP_EXAMPLES_VECTOR2D_HPP
#define LUACPP_EXAMPLES_VECTOR2D_HPP

#include <cmath>
#include <typeinfo>

class Vector2D {
public:
	float x, y;

	Vector2D(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}

	Vector2D operator+(const Vector2D& other) const {
		return Vector2D(x + other.x, y + other.y);
	}

	Vector2D operator-(const Vector2D& other) const {
		return Vector2D(x - other.x, y - other.y);
	}

	// Component-wise multiplication for scalar operations
	Vector2D operator*(const Vector2D& other) const {
		return Vector2D(x * other.x, y * other.y);
	}

	Vector2D operator-() const {
		return Vector2D(-x, -y);
	}

	bool operator==(const Vector2D& other) const {
		return std::abs(x - other.x) < 0.0001f && std::abs(y - other.y) < 0.0001f;
	}

	// Methods for future Lua exposure
	float length() const {
		return std::sqrt(x * x + y * y);
	}

	Vector2D normalize() const {
		float len = length();
		if (len > 0.0001f) {
			return Vector2D(x / len, y / len);
		}
		return Vector2D(0, 0);
	}

	float dot(const Vector2D& other) const {
		return x * other.x + y * other.y;
	}

	Vector2D rotate(float angleRad) const {
		float cosA = std::cos(angleRad);
		float sinA = std::sin(angleRad);
		return Vector2D(
			x * cosA - y * sinA,
			x * sinA + y * cosA
		);
	}
};

// ============================================================================
// Lua Metatable Specialization
// ============================================================================

// Forward declarations for Lua binding
namespace Lua {
	template <typename T> struct Metatable;
	class State;
	namespace detail { template <typename T> void registerDefaultMetatable(State&); }
}

namespace Lua {

template <>
struct Metatable<::Vector2D> {
	static const char* metatableName() {
		return typeid(::Vector2D).name();
	}

	static void registerMetatable(State& state) {
		detail::registerDefaultMetatable<::Vector2D>(state);
		registerConstructor(state);
	}

	template <typename... Args>
	static ::Vector2D* create(State& state, Args&&... args);

	static ::Vector2D* create(State& state, const ::Vector2D& obj);

	static void registerConstructor(State& state);
};

} // namespace Lua

// Include template implementations when State is fully defined
#ifdef LUACPP_STATE_HPP
namespace Lua {

template <typename... Args>
inline ::Vector2D* Metatable<::Vector2D>::create(State& state, Args&&... args) {
	::Vector2D* obj = state.createUserData<::Vector2D>(std::forward<Args>(args)...);
	state.assignMetaTable(metatableName());
	return obj;
}

inline ::Vector2D* Metatable<::Vector2D>::create(State& state, const ::Vector2D& obj) {
	::Vector2D* userdata = state.createUserData<::Vector2D>(obj);
	state.assignMetaTable(metatableName());
	return userdata;
}

} // namespace Lua
#endif

#endif // LUACPP_EXAMPLES_VECTOR2D_HPP
