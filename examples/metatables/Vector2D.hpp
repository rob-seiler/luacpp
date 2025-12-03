#ifndef LUACPP_EXAMPLES_VECTOR2D_HPP
#define LUACPP_EXAMPLES_VECTOR2D_HPP

#include <cmath>
#include <typeinfo>

class Vector2D {
public:
	// Configurable epsilon for floating-point comparisons
	// Users can adjust this based on their precision requirements
	static constexpr float EPSILON = 0.0001f;

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
		return std::abs(x - other.x) < EPSILON && std::abs(y - other.y) < EPSILON;
	}

	// Methods for future Lua exposure
	float length() const {
		return std::sqrt(x * x + y * y);
	}

	Vector2D normalize() const {
		float len = length();
		if (len > EPSILON) {
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

#endif // LUACPP_EXAMPLES_VECTOR2D_HPP
