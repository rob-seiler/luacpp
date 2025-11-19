-- Constructors are registered in C++, so we can use them directly!

print("--- Vector2D Operators ---")
print()

v1 = Vector(3, 4)
v2 = Vector(1, 2)

printVec(v1, "v1")
printVec(v2, "v2")
print()

print("Testing v1 + v2:")
sum = v1 + v2
printVec(sum, "sum")
print()

print("Testing v1 - v2:")
diff = v1 - v2
printVec(diff, "diff")
print()

print("Testing v1 * scalar (v1 * Vector(2.5, 2.5)):")
scalar = Vector(2.5, 2.5)
scaled = v1 * scalar
printVec(scaled, "scaled")
print()

print("Testing -v1:")
negated = -v1
printVec(negated, "negated")
print()

print("Testing equality:")
v3 = Vector(3, 4)
v4 = Vector(3.0001, 4.0001)
areEqual1 = (v1 == v3)
areEqual2 = (v1 == v4)
print("v1 == v3 (same values): " .. tostring(areEqual1))
print("v1 == v4 (slightly different): " .. tostring(areEqual2))
print()

print("--- Transform2D Operators ---")
print()

print("Creating transforms:")
t1 = Transform(
    Vector(10, 20),
    0,
    Vector(1, 1)
)
printTransform(t1, "t1")
print()

t2 = Transform(
    Vector(5, 5),
    1.57,
    Vector(2, 2)
)
printTransform(t2, "t2")
print()

print("Testing t1 * t2 (combine transforms):")
combined = t1 * t2
printTransform(combined, "combined")
print()

print("Testing transform equality:")
t3 = Transform(Vector(10, 20), 0, Vector(1, 1))
sameTransform = (t1 == t3)
print("t1 == t3 (same values): " .. tostring(sameTransform))
print()

print("--- Complex Expression ---")
print()

print("Testing: (v1 + v2) * Vector(0.5, 0.5) - v1")
half = Vector(0.5, 0.5)
result = (v1 + v2) * half - v1
printVec(result, "result")
print()

print("=== Script completed successfully! ===")
