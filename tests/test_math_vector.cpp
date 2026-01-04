#include <gtest/gtest.h>

#include "runtime/core/math/math.h"

namespace vesper {
namespace test {

// =============================================================================
// Vector2 Tests
// =============================================================================
class Vector2Test : public ::testing::Test {
protected:
    const float EPSILON = 1e-5f;
};

TEST_F(Vector2Test, DefaultConstructor) {
    Vector2 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST_F(Vector2Test, ParameterizedConstructor) {
    Vector2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.x, 3.0f);
    EXPECT_FLOAT_EQ(v.y, 4.0f);
}

TEST_F(Vector2Test, Addition) {
    Vector2 a(1.0f, 2.0f);
    Vector2 b(3.0f, 4.0f);
    Vector2 result = a + b;
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 6.0f);
}

TEST_F(Vector2Test, Subtraction) {
    Vector2 a(5.0f, 7.0f);
    Vector2 b(2.0f, 3.0f);
    Vector2 result = a - b;
    EXPECT_FLOAT_EQ(result.x, 3.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
}

TEST_F(Vector2Test, ScalarMultiplication) {
    Vector2 v(2.0f, 3.0f);
    Vector2 result = v * 2.0f;
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 6.0f);
}

TEST_F(Vector2Test, Length) {
    Vector2 v(3.0f, 4.0f);
    EXPECT_NEAR(v.length(), 5.0f, EPSILON);
}

TEST_F(Vector2Test, SquaredLength) {
    Vector2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.squaredLength(), 25.0f);
}

TEST_F(Vector2Test, DotProduct) {
    Vector2 a(3.0f, 4.0f);
    Vector2 b(1.0f, 2.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 11.0f);  // 3*1 + 4*2 = 11
}

TEST_F(Vector2Test, Normalize) {
    Vector2 v(3.0f, 4.0f);
    Vector2 normalized = v.normalized();
    EXPECT_NEAR(normalized.length(), 1.0f, EPSILON);
    EXPECT_NEAR(normalized.x, 0.6f, EPSILON);
    EXPECT_NEAR(normalized.y, 0.8f, EPSILON);
}

TEST_F(Vector2Test, Perpendicular) {
    Vector2 v(1.0f, 0.0f);
    Vector2 perp = v.perpendicular();
    EXPECT_FLOAT_EQ(perp.x, 0.0f);
    EXPECT_FLOAT_EQ(perp.y, 1.0f);
}

TEST_F(Vector2Test, Lerp) {
    Vector2 a(0.0f, 0.0f);
    Vector2 b(10.0f, 10.0f);
    Vector2 result = Vector2::lerp(a, b, 0.5f);
    EXPECT_NEAR(result.x, 5.0f, EPSILON);
    EXPECT_NEAR(result.y, 5.0f, EPSILON);
}

TEST_F(Vector2Test, StaticConstants) {
    EXPECT_FLOAT_EQ(Vector2::ZERO.x, 0.0f);
    EXPECT_FLOAT_EQ(Vector2::ZERO.y, 0.0f);
    EXPECT_FLOAT_EQ(Vector2::ONE.x, 1.0f);
    EXPECT_FLOAT_EQ(Vector2::ONE.y, 1.0f);
    EXPECT_FLOAT_EQ(Vector2::UNIT_X.x, 1.0f);
    EXPECT_FLOAT_EQ(Vector2::UNIT_X.y, 0.0f);
}

// =============================================================================
// Vector3 Tests
// =============================================================================
class Vector3Test : public ::testing::Test {
protected:
    const float EPSILON = 1e-5f;
};

TEST_F(Vector3Test, DefaultConstructor) {
    Vector3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST_F(Vector3Test, ParameterizedConstructor) {
    Vector3 v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST_F(Vector3Test, Addition) {
    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);
    Vector3 result = a + b;
    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 7.0f);
    EXPECT_FLOAT_EQ(result.z, 9.0f);
}

TEST_F(Vector3Test, CrossProduct) {
    Vector3 a(1.0f, 0.0f, 0.0f);  // X axis
    Vector3 b(0.0f, 1.0f, 0.0f);  // Y axis
    Vector3 result = a.cross(b);
    EXPECT_NEAR(result.x, 0.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 1.0f, EPSILON);  // Z axis
}

TEST_F(Vector3Test, CrossProductAntiCommutative) {
    Vector3 a(1.0f, 0.0f, 0.0f);
    Vector3 b(0.0f, 1.0f, 0.0f);
    Vector3 ab = a.cross(b);
    Vector3 ba = b.cross(a);
    EXPECT_NEAR(ab.x, -ba.x, EPSILON);
    EXPECT_NEAR(ab.y, -ba.y, EPSILON);
    EXPECT_NEAR(ab.z, -ba.z, EPSILON);
}

TEST_F(Vector3Test, DotProduct) {
    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 32.0f);  // 1*4 + 2*5 + 3*6 = 32
}

TEST_F(Vector3Test, Length) {
    Vector3 v(1.0f, 2.0f, 2.0f);
    EXPECT_NEAR(v.length(), 3.0f, EPSILON);  // sqrt(1 + 4 + 4) = 3
}

TEST_F(Vector3Test, Normalize) {
    Vector3 v(3.0f, 0.0f, 4.0f);
    Vector3 normalized = v.normalized();
    EXPECT_NEAR(normalized.length(), 1.0f, EPSILON);
    EXPECT_NEAR(normalized.x, 0.6f, EPSILON);
    EXPECT_NEAR(normalized.y, 0.0f, EPSILON);
    EXPECT_NEAR(normalized.z, 0.8f, EPSILON);
}

TEST_F(Vector3Test, Distance) {
    Vector3 a(0.0f, 0.0f, 0.0f);
    Vector3 b(3.0f, 4.0f, 0.0f);
    EXPECT_NEAR(a.distance(b), 5.0f, EPSILON);
}

TEST_F(Vector3Test, Reflect) {
    Vector3 incident(1.0f, -1.0f, 0.0f);
    Vector3 normal(0.0f, 1.0f, 0.0f);
    Vector3 reflected = incident.reflect(normal);
    EXPECT_NEAR(reflected.x, 1.0f, EPSILON);
    EXPECT_NEAR(reflected.y, 1.0f, EPSILON);
    EXPECT_NEAR(reflected.z, 0.0f, EPSILON);
}

TEST_F(Vector3Test, Lerp) {
    Vector3 a(0.0f, 0.0f, 0.0f);
    Vector3 b(10.0f, 20.0f, 30.0f);
    Vector3 result = Vector3::lerp(a, b, 0.5f);
    EXPECT_NEAR(result.x, 5.0f, EPSILON);
    EXPECT_NEAR(result.y, 10.0f, EPSILON);
    EXPECT_NEAR(result.z, 15.0f, EPSILON);
}

TEST_F(Vector3Test, StaticConstants) {
    EXPECT_EQ(Vector3::ZERO, Vector3(0, 0, 0));
    EXPECT_EQ(Vector3::ONE, Vector3(1, 1, 1));
    EXPECT_EQ(Vector3::UNIT_X, Vector3(1, 0, 0));
    EXPECT_EQ(Vector3::UNIT_Y, Vector3(0, 1, 0));
    EXPECT_EQ(Vector3::UNIT_Z, Vector3(0, 0, 1));
    EXPECT_EQ(Vector3::UP, Vector3(0, 1, 0));
}

// =============================================================================
// Vector4 Tests
// =============================================================================
class Vector4Test : public ::testing::Test {
protected:
    const float EPSILON = 1e-5f;
};

TEST_F(Vector4Test, DefaultConstructor) {
    Vector4 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
    EXPECT_FLOAT_EQ(v.w, 0.0f);
}

TEST_F(Vector4Test, ParameterizedConstructor) {
    Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
    EXPECT_FLOAT_EQ(v.w, 4.0f);
}

TEST_F(Vector4Test, ConstructFromVector3) {
    Vector3 v3(1.0f, 2.0f, 3.0f);
    Vector4 v4(v3, 1.0f);
    EXPECT_FLOAT_EQ(v4.x, 1.0f);
    EXPECT_FLOAT_EQ(v4.y, 2.0f);
    EXPECT_FLOAT_EQ(v4.z, 3.0f);
    EXPECT_FLOAT_EQ(v4.w, 1.0f);
}

TEST_F(Vector4Test, DotProduct) {
    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 b(4.0f, 3.0f, 2.0f, 1.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 20.0f);  // 1*4 + 2*3 + 3*2 + 4*1 = 20
}

TEST_F(Vector4Test, GetXYZ) {
    Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    Vector3 xyz = v.xyz();
    EXPECT_FLOAT_EQ(xyz.x, 1.0f);
    EXPECT_FLOAT_EQ(xyz.y, 2.0f);
    EXPECT_FLOAT_EQ(xyz.z, 3.0f);
}

TEST_F(Vector4Test, Normalize) {
    Vector4 v(1.0f, 0.0f, 0.0f, 0.0f);
    Vector4 normalized = v.normalized();
    EXPECT_NEAR(normalized.length(), 1.0f, EPSILON);
}

TEST_F(Vector4Test, Lerp) {
    Vector4 a(0.0f, 0.0f, 0.0f, 0.0f);
    Vector4 b(10.0f, 20.0f, 30.0f, 40.0f);
    Vector4 result = Vector4::lerp(a, b, 0.5f);
    EXPECT_NEAR(result.x, 5.0f, EPSILON);
    EXPECT_NEAR(result.y, 10.0f, EPSILON);
    EXPECT_NEAR(result.z, 15.0f, EPSILON);
    EXPECT_NEAR(result.w, 20.0f, EPSILON);
}

} // namespace test
} // namespace vesper