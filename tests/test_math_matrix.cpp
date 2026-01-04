#include <gtest/gtest.h>

#include "runtime/core/math/math.h"

namespace vesper {
namespace test {

class Matrix4x4Test : public ::testing::Test {
protected:
    const float EPSILON = 1e-4f;
};

TEST_F(Matrix4x4Test, DefaultConstructorIsIdentity) {
    Matrix4x4 m;
    EXPECT_FLOAT_EQ(m[0][0], 1.0f);
    EXPECT_FLOAT_EQ(m[1][1], 1.0f);
    EXPECT_FLOAT_EQ(m[2][2], 1.0f);
    EXPECT_FLOAT_EQ(m[3][3], 1.0f);
    EXPECT_FLOAT_EQ(m[0][1], 0.0f);
    EXPECT_FLOAT_EQ(m[1][0], 0.0f);
}

TEST_F(Matrix4x4Test, IdentityConstant) {
    Matrix4x4 identity = Matrix4x4::IDENTITY;
    EXPECT_FLOAT_EQ(identity[0][0], 1.0f);
    EXPECT_FLOAT_EQ(identity[1][1], 1.0f);
    EXPECT_FLOAT_EQ(identity[2][2], 1.0f);
    EXPECT_FLOAT_EQ(identity[3][3], 1.0f);
}

TEST_F(Matrix4x4Test, ZeroConstant) {
    Matrix4x4 zero = Matrix4x4::ZERO;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(zero[i][j], 0.0f);
        }
    }
}

TEST_F(Matrix4x4Test, Translation) {
    Matrix4x4 trans = Matrix4x4::translation(1.0f, 2.0f, 3.0f);
    Vector3 point(0.0f, 0.0f, 0.0f);
    Vector3 result = trans.transformPoint(point);

    EXPECT_NEAR(result.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.y, 2.0f, EPSILON);
    EXPECT_NEAR(result.z, 3.0f, EPSILON);
}

TEST_F(Matrix4x4Test, TranslationWithVector) {
    Vector3 offset(5.0f, 10.0f, 15.0f);
    Matrix4x4 trans = Matrix4x4::translation(offset);
    Vector3 point(1.0f, 1.0f, 1.0f);
    Vector3 result = trans.transformPoint(point);

    EXPECT_NEAR(result.x, 6.0f, EPSILON);
    EXPECT_NEAR(result.y, 11.0f, EPSILON);
    EXPECT_NEAR(result.z, 16.0f, EPSILON);
}

TEST_F(Matrix4x4Test, Scaling) {
    Matrix4x4 scale = Matrix4x4::scaling(2.0f, 3.0f, 4.0f);
    Vector3 point(1.0f, 1.0f, 1.0f);
    Vector3 result = scale.transformPoint(point);

    EXPECT_NEAR(result.x, 2.0f, EPSILON);
    EXPECT_NEAR(result.y, 3.0f, EPSILON);
    EXPECT_NEAR(result.z, 4.0f, EPSILON);
}

TEST_F(Matrix4x4Test, UniformScaling) {
    Matrix4x4 scale = Matrix4x4::scaling(2.0f);
    Vector3 point(1.0f, 2.0f, 3.0f);
    Vector3 result = scale.transformPoint(point);

    EXPECT_NEAR(result.x, 2.0f, EPSILON);
    EXPECT_NEAR(result.y, 4.0f, EPSILON);
    EXPECT_NEAR(result.z, 6.0f, EPSILON);
}

TEST_F(Matrix4x4Test, RotationX) {
    Matrix4x4 rot = Matrix4x4::rotationX(Math::HALF_PI);
    Vector3 point(0.0f, 1.0f, 0.0f);  // Y axis
    Vector3 result = rot.transformPoint(point);

    // Y axis rotated 90 degrees around X -> Z axis
    EXPECT_NEAR(result.x, 0.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 1.0f, EPSILON);
}

TEST_F(Matrix4x4Test, RotationY) {
    Matrix4x4 rot = Matrix4x4::rotationY(Math::HALF_PI);
    Vector3 point(0.0f, 0.0f, 1.0f);  // Z axis
    Vector3 result = rot.transformPoint(point);

    // Z axis rotated 90 degrees around Y -> X axis
    EXPECT_NEAR(result.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, RotationZ) {
    Matrix4x4 rot = Matrix4x4::rotationZ(Math::HALF_PI);
    Vector3 point(1.0f, 0.0f, 0.0f);  // X axis
    Vector3 result = rot.transformPoint(point);

    // X axis rotated 90 degrees around Z -> Y axis
    EXPECT_NEAR(result.x, 0.0f, EPSILON);
    EXPECT_NEAR(result.y, 1.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, RotationQuaternion) {
    Quaternion q = Quaternion::rotationY(Math::HALF_PI);
    Matrix4x4 rot = Matrix4x4::rotationQuaternion(q);
    Vector3 point(0.0f, 0.0f, 1.0f);
    Vector3 result = rot.transformPoint(point);

    EXPECT_NEAR(result.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, TRS) {
    Vector3 translation(10.0f, 0.0f, 0.0f);
    Quaternion rotation = Quaternion::IDENTITY;
    Vector3 scale(2.0f, 2.0f, 2.0f);

    Matrix4x4 trs = Matrix4x4::TRS(translation, rotation, scale);
    Vector3 point(1.0f, 0.0f, 0.0f);
    Vector3 result = trs.transformPoint(point);

    // Scale by 2, then translate by 10: 1*2 + 10 = 12
    EXPECT_NEAR(result.x, 12.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, MatrixMultiplication) {
    Matrix4x4 trans = Matrix4x4::translation(5.0f, 0.0f, 0.0f);
    Matrix4x4 scale = Matrix4x4::scaling(2.0f, 2.0f, 2.0f);

    // DXM: row-major, left-to-right multiplication
    // scale * trans: first scale, then translate
    Matrix4x4 combined = scale * trans;

    Vector3 point(1.0f, 0.0f, 0.0f);
    Vector3 result = combined.transformPoint(point);

    // 1 * 2 = 2, then + 5 = 7
    EXPECT_NEAR(result.x, 7.0f, EPSILON);
}

TEST_F(Matrix4x4Test, Transpose) {
    Matrix4x4 m(
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    );
    Matrix4x4 t = m.transpose();

    EXPECT_FLOAT_EQ(t[0][1], 5.0f);
    EXPECT_FLOAT_EQ(t[1][0], 2.0f);
    EXPECT_FLOAT_EQ(t[2][3], 15.0f);
    EXPECT_FLOAT_EQ(t[3][2], 12.0f);
}

TEST_F(Matrix4x4Test, Inverse) {
    Matrix4x4 trans = Matrix4x4::translation(1.0f, 2.0f, 3.0f);
    Matrix4x4 inv = trans.inverse();

    Vector3 point(1.0f, 2.0f, 3.0f);
    Vector3 result = inv.transformPoint(point);

    EXPECT_NEAR(result.x, 0.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, InverseMultiplication) {
    Matrix4x4 m = Matrix4x4::TRS(
        Vector3(1, 2, 3),
        Quaternion::rotationY(Math::PI / 4.0f),
        Vector3(2, 2, 2)
    );
    Matrix4x4 inv = m.inverse();
    Matrix4x4 result = m * inv;

    // Should be identity
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_NEAR(result[i][j], expected, EPSILON);
        }
    }
}

TEST_F(Matrix4x4Test, Determinant) {
    Matrix4x4 identity = Matrix4x4::IDENTITY;
    EXPECT_NEAR(identity.determinant(), 1.0f, EPSILON);

    Matrix4x4 scale = Matrix4x4::scaling(2.0f, 3.0f, 4.0f);
    EXPECT_NEAR(scale.determinant(), 24.0f, EPSILON);  // 2 * 3 * 4 = 24
}

TEST_F(Matrix4x4Test, GetTranslation) {
    Matrix4x4 trans = Matrix4x4::translation(5.0f, 10.0f, 15.0f);
    Vector3 t = trans.getTranslation();

    EXPECT_NEAR(t.x, 5.0f, EPSILON);
    EXPECT_NEAR(t.y, 10.0f, EPSILON);
    EXPECT_NEAR(t.z, 15.0f, EPSILON);
}

TEST_F(Matrix4x4Test, SetTranslation) {
    Matrix4x4 m = Matrix4x4::IDENTITY;
    m.setTranslation(Vector3(1.0f, 2.0f, 3.0f));

    Vector3 point(0.0f, 0.0f, 0.0f);
    Vector3 result = m.transformPoint(point);

    EXPECT_NEAR(result.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.y, 2.0f, EPSILON);
    EXPECT_NEAR(result.z, 3.0f, EPSILON);
}

TEST_F(Matrix4x4Test, GetScale) {
    Matrix4x4 scale = Matrix4x4::scaling(2.0f, 3.0f, 4.0f);
    Vector3 s = scale.getScale();

    EXPECT_NEAR(s.x, 2.0f, EPSILON);
    EXPECT_NEAR(s.y, 3.0f, EPSILON);
    EXPECT_NEAR(s.z, 4.0f, EPSILON);
}

TEST_F(Matrix4x4Test, Decompose) {
    Vector3 expectedT(5.0f, 10.0f, 15.0f);
    Quaternion expectedR = Quaternion::rotationY(Math::PI / 4.0f);
    Vector3 expectedS(1.0f, 2.0f, 3.0f);

    Matrix4x4 m = Matrix4x4::TRS(expectedT, expectedR, expectedS);

    Vector3 t, s;
    Quaternion r;
    m.decompose(t, r, s);

    EXPECT_NEAR(t.x, expectedT.x, EPSILON);
    EXPECT_NEAR(t.y, expectedT.y, EPSILON);
    EXPECT_NEAR(t.z, expectedT.z, EPSILON);

    EXPECT_NEAR(s.x, expectedS.x, EPSILON);
    EXPECT_NEAR(s.y, expectedS.y, EPSILON);
    EXPECT_NEAR(s.z, expectedS.z, EPSILON);

    // Quaternions can be negated and still represent same rotation
    float dot = std::abs(r.dot(expectedR));
    EXPECT_NEAR(dot, 1.0f, EPSILON);
}

TEST_F(Matrix4x4Test, TransformPoint) {
    Matrix4x4 trans = Matrix4x4::translation(1.0f, 2.0f, 3.0f);
    Vector3 point(0.0f, 0.0f, 0.0f);
    Vector3 result = trans.transformPoint(point);

    EXPECT_NEAR(result.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.y, 2.0f, EPSILON);
    EXPECT_NEAR(result.z, 3.0f, EPSILON);
}

TEST_F(Matrix4x4Test, TransformDirection) {
    Matrix4x4 trans = Matrix4x4::translation(100.0f, 100.0f, 100.0f);
    Vector3 dir(1.0f, 0.0f, 0.0f);
    Vector3 result = trans.transformDirection(dir);

    // Direction should not be affected by translation
    EXPECT_NEAR(result.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, LookAtLH) {
    Vector3 eye(0.0f, 0.0f, -5.0f);
    Vector3 target(0.0f, 0.0f, 0.0f);
    Vector3 up(0.0f, 1.0f, 0.0f);

    Matrix4x4 view = Matrix4x4::lookAtLH(eye, target, up);

    // Transform eye position should result in origin
    Vector3 result = view.transformPoint(eye);
    EXPECT_NEAR(result.x, 0.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
}

TEST_F(Matrix4x4Test, PerspectiveFovLH) {
    float fov = Math::PI / 4.0f;  // 45 degrees
    float aspect = 16.0f / 9.0f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    Matrix4x4 proj = Matrix4x4::perspectiveFovLH(fov, aspect, nearZ, farZ);

    // Perspective matrix should have specific structure
    EXPECT_NE(proj[0][0], 0.0f);
    EXPECT_NE(proj[1][1], 0.0f);
    EXPECT_NE(proj[2][2], 0.0f);
    EXPECT_NE(proj[2][3], 0.0f);
}

TEST_F(Matrix4x4Test, OrthographicLH) {
    float width = 800.0f;
    float height = 600.0f;
    float nearZ = 0.1f;
    float farZ = 100.0f;

    Matrix4x4 ortho = Matrix4x4::orthographicLH(width, height, nearZ, farZ);

    // Orthographic matrix diagonal elements
    EXPECT_NEAR(ortho[0][0], 2.0f / width, EPSILON);
    EXPECT_NEAR(ortho[1][1], 2.0f / height, EPSILON);
}

// =============================================================================
// Math Utility Tests
// =============================================================================
class MathUtilTest : public ::testing::Test {
protected:
    const float EPSILON = 1e-5f;
};

TEST_F(MathUtilTest, Constants) {
    EXPECT_NEAR(Math::PI, 3.14159265f, 1e-5f);
    EXPECT_NEAR(Math::TWO_PI, 2.0f * Math::PI, EPSILON);
    EXPECT_NEAR(Math::HALF_PI, Math::PI / 2.0f, EPSILON);
    EXPECT_NEAR(Math::DEG_TO_RAD, Math::PI / 180.0f, EPSILON);
    EXPECT_NEAR(Math::RAD_TO_DEG, 180.0f / Math::PI, EPSILON);
}

TEST_F(MathUtilTest, DegreesToRadians) {
    EXPECT_NEAR(Math::degreesToRadians(0.0f), 0.0f, EPSILON);
    EXPECT_NEAR(Math::degreesToRadians(90.0f), Math::HALF_PI, EPSILON);
    EXPECT_NEAR(Math::degreesToRadians(180.0f), Math::PI, EPSILON);
    EXPECT_NEAR(Math::degreesToRadians(360.0f), Math::TWO_PI, EPSILON);
}

TEST_F(MathUtilTest, RadiansToDegrees) {
    EXPECT_NEAR(Math::radiansToDegrees(0.0f), 0.0f, EPSILON);
    EXPECT_NEAR(Math::radiansToDegrees(Math::HALF_PI), 90.0f, EPSILON);
    EXPECT_NEAR(Math::radiansToDegrees(Math::PI), 180.0f, EPSILON);
    EXPECT_NEAR(Math::radiansToDegrees(Math::TWO_PI), 360.0f, EPSILON);
}

TEST_F(MathUtilTest, Clamp) {
    EXPECT_FLOAT_EQ(Math::clamp(0.5f, 0.0f, 1.0f), 0.5f);
    EXPECT_FLOAT_EQ(Math::clamp(-1.0f, 0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(Math::clamp(2.0f, 0.0f, 1.0f), 1.0f);
}

TEST_F(MathUtilTest, Lerp) {
    EXPECT_FLOAT_EQ(Math::lerp(0.0f, 10.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Math::lerp(0.0f, 10.0f, 1.0f), 10.0f);
    EXPECT_FLOAT_EQ(Math::lerp(0.0f, 10.0f, 0.5f), 5.0f);
    EXPECT_FLOAT_EQ(Math::lerp(0.0f, 10.0f, 0.3f), 3.0f);
}

TEST_F(MathUtilTest, TrigFunctions) {
    EXPECT_NEAR(Math::sin(0.0f), 0.0f, EPSILON);
    EXPECT_NEAR(Math::sin(Math::HALF_PI), 1.0f, EPSILON);
    EXPECT_NEAR(Math::cos(0.0f), 1.0f, EPSILON);
    EXPECT_NEAR(Math::cos(Math::PI), -1.0f, EPSILON);
    EXPECT_NEAR(Math::tan(0.0f), 0.0f, EPSILON);
}

TEST_F(MathUtilTest, InverseTrigFunctions) {
    EXPECT_NEAR(Math::asin(0.0f), 0.0f, EPSILON);
    EXPECT_NEAR(Math::asin(1.0f), Math::HALF_PI, EPSILON);
    EXPECT_NEAR(Math::acos(1.0f), 0.0f, EPSILON);
    EXPECT_NEAR(Math::acos(0.0f), Math::HALF_PI, EPSILON);
}

TEST_F(MathUtilTest, MinMax) {
    EXPECT_EQ(Math::min(1, 2), 1);
    EXPECT_EQ(Math::max(1, 2), 2);
    EXPECT_EQ(Math::min3(1, 2, 3), 1);
    EXPECT_EQ(Math::max3(1, 2, 3), 3);
}

TEST_F(MathUtilTest, Sqrt) {
    EXPECT_NEAR(Math::sqrt(4.0f), 2.0f, EPSILON);
    EXPECT_NEAR(Math::sqrt(9.0f), 3.0f, EPSILON);
    EXPECT_NEAR(Math::sqrt(2.0f), 1.41421356f, EPSILON);
}

TEST_F(MathUtilTest, Abs) {
    EXPECT_FLOAT_EQ(Math::abs(5.0f), 5.0f);
    EXPECT_FLOAT_EQ(Math::abs(-5.0f), 5.0f);
    EXPECT_FLOAT_EQ(Math::abs(0.0f), 0.0f);
}

TEST_F(MathUtilTest, RealEqual) {
    EXPECT_TRUE(Math::realEqual(1.0f, 1.0f));
    EXPECT_TRUE(Math::realEqual(1.0f, 1.0f + 1e-8f));
    EXPECT_FALSE(Math::realEqual(1.0f, 2.0f));
    EXPECT_TRUE(Math::realEqual(1.0f, 1.1f, 0.2f));
}

// =============================================================================
// Radian/Degree Tests
// =============================================================================
class AngleTest : public ::testing::Test {
protected:
    const float EPSILON = 1e-5f;
};

TEST_F(AngleTest, RadianConstruction) {
    Radian r(Math::PI);
    EXPECT_NEAR(r.valueRadians(), Math::PI, EPSILON);
    EXPECT_NEAR(r.valueDegrees(), 180.0f, EPSILON);
}

TEST_F(AngleTest, DegreeConstruction) {
    Degree d(180.0f);
    EXPECT_NEAR(d.valueDegrees(), 180.0f, EPSILON);
    EXPECT_NEAR(d.valueRadians(), Math::PI, EPSILON);
}

TEST_F(AngleTest, RadianFromDegree) {
    Degree d(90.0f);
    Radian r(d);
    EXPECT_NEAR(r.valueRadians(), Math::HALF_PI, EPSILON);
}

TEST_F(AngleTest, RadianArithmetic) {
    Radian a(Math::PI);
    Radian b(Math::HALF_PI);

    Radian sum = a + b;
    EXPECT_NEAR(sum.valueRadians(), Math::PI + Math::HALF_PI, EPSILON);

    Radian diff = a - b;
    EXPECT_NEAR(diff.valueRadians(), Math::HALF_PI, EPSILON);

    Radian scaled = a * 2.0f;
    EXPECT_NEAR(scaled.valueRadians(), Math::TWO_PI, EPSILON);
}

TEST_F(AngleTest, DegreeArithmetic) {
    Degree a(180.0f);
    Degree b(90.0f);

    Degree sum = a + b;
    EXPECT_NEAR(sum.valueDegrees(), 270.0f, EPSILON);

    Degree diff = a - b;
    EXPECT_NEAR(diff.valueDegrees(), 90.0f, EPSILON);

    Degree scaled = a * 2.0f;
    EXPECT_NEAR(scaled.valueDegrees(), 360.0f, EPSILON);
}

} // namespace test
} // namespace vesper
