#pragma once

/// @file math.h
/// @brief Main include file for VesperEngine math library
/// @details This math library is built on top of DirectXMath (DXM).
///
/// Key differences from GLM/Piccolo:
/// - Uses row-major matrix storage (like Direct3D)
/// - Uses left-to-right matrix multiplication: v * M1 * M2 * M3
/// - Quaternion format: (x, y, z, w)
/// - Left-handed coordinate system by default (can use RH variants)
///
/// Example usage:
/// @code
///     using namespace vesper;
///
///     Vector3 position(0, 0, 5);
///     Quaternion rotation = Quaternion::rotationY(Math::PI / 4);
///     Matrix4x4 transform = Matrix4x4::TRS(position, rotation, Vector3::ONE);
///
///     Vector3 worldPos = transform.transformPoint(localPos);
/// @endcode

#include "runtime/core/math/math_types.h"
#include "runtime/core/math/vector2.h"
#include "runtime/core/math/vector3.h"
#include "runtime/core/math/vector4.h"
#include "runtime/core/math/quaternion.h"
#include "runtime/core/math/matrix4x4.h"