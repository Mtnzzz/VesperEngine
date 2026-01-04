#include <gtest/gtest.h>

#include "runtime/core/math/math.h"

namespace vesper {
namespace test {

class QuaternionTest : public ::testing::Test {
protected:
    const float EPSILON = 1e-4f;
};

TEST_F(QuaternionTest, DefaultConstructor) {
    Quaternion q;
    EXPECT_FLOAT_EQ(q.x, 0.0f);
    EXPECT_FLOAT_EQ(q.y, 0.0f);
    EXPECT_FLOAT_EQ(q.z, 0.0f);
    EXPECT_FLOAT_EQ(q.w, 1.0f);
}

TEST_F(QuaternionTest, IdentityQuaternion) {
    Quaternion identity = Quaternion::IDENTITY;
    EXPECT_FLOAT_EQ(identity.x, 0.0f);
    EXPECT_FLOAT_EQ(identity.y, 0.0f);
    EXPECT_FLOAT_EQ(identity.z, 0.0f);
    EXPECT_FLOAT_EQ(identity.w, 1.0f);
    EXPECT_TRUE(identity.isIdentity());
}

TEST_F(QuaternionTest, RotationX90) {
    Quaternion q = Quaternion::rotationX(Math::HALF_PI);
    Vector3 v(0.0f, 1.0f, 0.0f);  // Y axis
    Vector3 rotated = q * v;

    // Y axis rotated 90 degrees around X should become Z axis
    EXPECT_NEAR(rotated.x, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.y, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.z, 1.0f, EPSILON);
}

TEST_F(QuaternionTest, RotationY90) {
    Quaternion q = Quaternion::rotationY(Math::HALF_PI);
    Vector3 v(0.0f, 0.0f, 1.0f);  // Z axis
    Vector3 rotated = q * v;

    // Z axis rotated 90 degrees around Y should become X axis
    EXPECT_NEAR(rotated.x, 1.0f, EPSILON);
    EXPECT_NEAR(rotated.y, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.z, 0.0f, EPSILON);
}

TEST_F(QuaternionTest, RotationZ90) {
    Quaternion q = Quaternion::rotationZ(Math::HALF_PI);
    Vector3 v(1.0f, 0.0f, 0.0f);  // X axis
    Vector3 rotated = q * v;

    // X axis rotated 90 degrees around Z should become Y axis
    EXPECT_NEAR(rotated.x, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.y, 1.0f, EPSILON);
    EXPECT_NEAR(rotated.z, 0.0f, EPSILON);
}

TEST_F(QuaternionTest, IdentityDoesNotRotate) {
    Quaternion identity = Quaternion::IDENTITY;
    Vector3 v(1.0f, 2.0f, 3.0f);
    Vector3 rotated = identity * v;

    EXPECT_NEAR(rotated.x, v.x, EPSILON);
    EXPECT_NEAR(rotated.y, v.y, EPSILON);
    EXPECT_NEAR(rotated.z, v.z, EPSILON);
}

TEST_F(QuaternionTest, QuaternionMultiplication) {
    // Two 90 degree rotations around Y should equal 180 degree rotation
    Quaternion q90 = Quaternion::rotationY(Math::HALF_PI);
    Quaternion q180 = q90 * q90;

    Vector3 forward(0.0f, 0.0f, 1.0f);
    Vector3 rotated = q180 * forward;

    // 180 degree rotation around Y: (0,0,1) -> (0,0,-1)
    EXPECT_NEAR(rotated.x, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.y, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.z, -1.0f, EPSILON);
}

TEST_F(QuaternionTest, Conjugate) {
    Quaternion q(0.5f, 0.5f, 0.5f, 0.5f);
    Quaternion conj = q.conjugate();

    EXPECT_FLOAT_EQ(conj.x, -0.5f);
    EXPECT_FLOAT_EQ(conj.y, -0.5f);
    EXPECT_FLOAT_EQ(conj.z, -0.5f);
    EXPECT_FLOAT_EQ(conj.w, 0.5f);
}

TEST_F(QuaternionTest, Inverse) {
    Quaternion q = Quaternion::rotationY(Math::PI / 4.0f);
    Quaternion inv = q.inverse();
    Quaternion result = q * inv;

    // q * q^-1 should be identity
    EXPECT_NEAR(result.x, 0.0f, EPSILON);
    EXPECT_NEAR(result.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.z, 0.0f, EPSILON);
    EXPECT_NEAR(result.w, 1.0f, EPSILON);
}

TEST_F(QuaternionTest, Normalize) {
    Quaternion q(1.0f, 2.0f, 3.0f, 4.0f);
    Quaternion normalized = q.normalized();
    EXPECT_NEAR(normalized.length(), 1.0f, EPSILON);
}

TEST_F(QuaternionTest, DotProduct) {
    Quaternion a(1.0f, 0.0f, 0.0f, 0.0f);
    Quaternion b(1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_NEAR(a.dot(b), 1.0f, EPSILON);

    Quaternion c(0.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_NEAR(a.dot(c), 0.0f, EPSILON);
}

TEST_F(QuaternionTest, Slerp) {
    Quaternion q1 = Quaternion::IDENTITY;
    Quaternion q2 = Quaternion::rotationY(Math::PI);

    // Slerp at t=0 should be q1
    Quaternion result0 = Quaternion::slerp(q1, q2, 0.0f);
    EXPECT_NEAR(result0.x, q1.x, EPSILON);
    EXPECT_NEAR(result0.y, q1.y, EPSILON);
    EXPECT_NEAR(result0.z, q1.z, EPSILON);
    EXPECT_NEAR(result0.w, q1.w, EPSILON);

    // Slerp at t=1 should be q2
    Quaternion result1 = Quaternion::slerp(q1, q2, 1.0f);
    // Note: slerp may return -q2 which is equivalent
    EXPECT_NEAR(std::abs(result1.dot(q2)), 1.0f, EPSILON);
}

TEST_F(QuaternionTest, SlerpMidpoint) {
    Quaternion q1 = Quaternion::IDENTITY;
    Quaternion q2 = Quaternion::rotationY(Math::PI);

    Quaternion mid = Quaternion::slerp(q1, q2, 0.5f);

    // Midpoint should be 90 degree rotation
    Vector3 forward(0.0f, 0.0f, 1.0f);
    Vector3 rotated = mid * forward;

    // Should be approximately (1, 0, 0) or (-1, 0, 0)
    EXPECT_NEAR(rotated.y, 0.0f, EPSILON);
    EXPECT_NEAR(std::abs(rotated.x), 1.0f, EPSILON);
}

TEST_F(QuaternionTest, FromAngleAxis) {
    Vector3 axis(0.0f, 1.0f, 0.0f);  // Y axis
    Radian angle(Math::HALF_PI);     // 90 degrees
    Quaternion q(angle, axis);

    Vector3 forward(0.0f, 0.0f, 1.0f);
    Vector3 rotated = q * forward;

    EXPECT_NEAR(rotated.x, 1.0f, EPSILON);
    EXPECT_NEAR(rotated.y, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.z, 0.0f, EPSILON);
}

TEST_F(QuaternionTest, ToAngleAxis) {
    Quaternion q = Quaternion::rotationY(Math::HALF_PI);

    Radian angle;
    Vector3 axis;
    q.toAngleAxis(angle, axis);

    EXPECT_NEAR(angle.valueRadians(), Math::HALF_PI, EPSILON);
    EXPECT_NEAR(std::abs(axis.y), 1.0f, EPSILON);
}

TEST_F(QuaternionTest, GetAxes) {
    Quaternion identity = Quaternion::IDENTITY;

    EXPECT_NEAR(identity.getAxisX().x, 1.0f, EPSILON);
    EXPECT_NEAR(identity.getAxisY().y, 1.0f, EPSILON);
    EXPECT_NEAR(identity.getAxisZ().z, 1.0f, EPSILON);
}

TEST_F(QuaternionTest, FromEulerAngles) {
    // Test 90 degree yaw (rotation around Y)
    Quaternion q = Quaternion::fromEulerAngles(0.0f, Math::HALF_PI, 0.0f);

    Vector3 forward(0.0f, 0.0f, 1.0f);
    Vector3 rotated = q * forward;

    EXPECT_NEAR(rotated.x, 1.0f, EPSILON);
    EXPECT_NEAR(rotated.y, 0.0f, EPSILON);
    EXPECT_NEAR(rotated.z, 0.0f, EPSILON);
}

} // namespace test
} // namespace vesper