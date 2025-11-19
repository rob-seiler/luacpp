#ifndef LUACPP_EXAMPLES_TRANSFORM2D_HPP
#define LUACPP_EXAMPLES_TRANSFORM2D_HPP

#include "Vector2D.hpp"
#include <typeinfo>

class Transform2D {
public:
    Vector2D position;
    float rotation;  // in radians
    Vector2D scale;

    Transform2D(const Vector2D& pos = Vector2D(0, 0),
                float rot = 0.0f,
                const Vector2D& scl = Vector2D(1, 1))
        : position(pos), rotation(rot), scale(scl) {}

    // Compose transforms: first apply 'other', then apply 'this'
    Transform2D operator*(const Transform2D& other) const {
        // Apply other's scale and rotation to this position
        Vector2D rotatedPos = other.position.rotate(rotation);
        Vector2D scaledPos = Vector2D(rotatedPos.x * scale.x, rotatedPos.y * scale.y);
        Vector2D newPos = position + scaledPos;

        float newRot = rotation + other.rotation;
        Vector2D newScale(scale.x * other.scale.x, scale.y * other.scale.y);

        return Transform2D(newPos, newRot, newScale);
    }

    bool operator==(const Transform2D& other) const {
        return position == other.position &&
               std::abs(rotation - other.rotation) < 0.0001f &&
               scale == other.scale;
    }

    // Methods for future Lua exposure
    Vector2D apply(const Vector2D& vec) const {
        Vector2D scaled(vec.x * scale.x, vec.y * scale.y);
        Vector2D rotated = scaled.rotate(rotation);
        return position + rotated;
    }

    Transform2D inverse() const {
        Vector2D invScale(1.0f / scale.x, 1.0f / scale.y);
        float invRot = -rotation;

        Vector2D invPos = position.rotate(-rotation);
        invPos = Vector2D(-invPos.x * invScale.x, -invPos.y * invScale.y);

        return Transform2D(invPos, invRot, invScale);
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
struct Metatable<::Transform2D> {
	static const char* metatableName() {
		return typeid(::Transform2D).name();
	}

	static void registerMetatable(State& state) {
		detail::registerDefaultMetatable<::Transform2D>(state);
		registerConstructor(state);
	}

	template <typename... Args>
	static ::Transform2D* create(State& state, Args&&... args);

	static ::Transform2D* create(State& state, const ::Transform2D& obj);

	static void registerConstructor(State& state);
};

} // namespace Lua

// Include template implementations when State is fully defined
#ifdef LUACPP_STATE_HPP
namespace Lua {

template <typename... Args>
inline ::Transform2D* Metatable<::Transform2D>::create(State& state, Args&&... args) {
	::Transform2D* obj = state.createUserData<::Transform2D>(std::forward<Args>(args)...);
	state.assignMetaTable(metatableName());
	return obj;
}

inline ::Transform2D* Metatable<::Transform2D>::create(State& state, const ::Transform2D& obj) {
	::Transform2D* userdata = state.createUserData<::Transform2D>(obj);
	state.assignMetaTable(metatableName());
	return userdata;
}

} // namespace Lua
#endif

#endif // LUACPP_EXAMPLES_TRANSFORM2D_HPP
