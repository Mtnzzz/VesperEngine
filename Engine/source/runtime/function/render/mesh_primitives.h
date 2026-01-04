#pragma once

#include "runtime/function/render/mesh.h"
#include "runtime/core/math/vector2.h"
#include "runtime/core/math/vector3.h"

namespace vesper {

// =============================================================================
// Vertex Format Definitions
// =============================================================================

/// @brief Standard vertex format (Position + Normal + UV)
/// Used for most 3D rendering with lighting and texturing
struct StandardVertex
{
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;

    StandardVertex() = default;
    StandardVertex(const Vector3& pos, const Vector3& norm, const Vector2& uv)
        : position(pos), normal(norm), texCoord(uv) {}

    static RHIVertexInputState getVertexLayout()
    {
        RHIVertexInputState state;
        state.bindings.push_back({
            0,                          // binding
            sizeof(StandardVertex),     // stride
            RHIVertexInputRate::Vertex  // input rate
        });
        state.attributes.push_back({
            0,                      // location (POSITION)
            0,                      // binding
            RHIFormat::RGB32_FLOAT, // format
            offsetof(StandardVertex, position)
        });
        state.attributes.push_back({
            1,                      // location (NORMAL)
            0,                      // binding
            RHIFormat::RGB32_FLOAT, // format
            offsetof(StandardVertex, normal)
        });
        state.attributes.push_back({
            2,                      // location (TEXCOORD)
            0,                      // binding
            RHIFormat::RG32_FLOAT,  // format
            offsetof(StandardVertex, texCoord)
        });
        return state;
    }
};

/// @brief Color vertex format (Position + Color)
/// Used for debugging and simple colored geometry
struct ColorVertex
{
    Vector3 position;
    Vector3 color;

    ColorVertex() = default;
    ColorVertex(const Vector3& pos, const Vector3& col)
        : position(pos), color(col) {}

    static RHIVertexInputState getVertexLayout()
    {
        RHIVertexInputState state;
        state.bindings.push_back({
            0,                          // binding
            sizeof(ColorVertex),        // stride
            RHIVertexInputRate::Vertex  // input rate
        });
        state.attributes.push_back({
            0,                      // location (POSITION)
            0,                      // binding
            RHIFormat::RGB32_FLOAT, // format
            offsetof(ColorVertex, position)
        });
        state.attributes.push_back({
            1,                      // location (COLOR)
            0,                      // binding
            RHIFormat::RGB32_FLOAT, // format
            offsetof(ColorVertex, color)
        });
        return state;
    }
};

// =============================================================================
// Primitive Factory Functions
// =============================================================================

/// @brief Create a colored cube mesh
/// @param size Size of the cube (side length)
/// @return MeshData with ColorVertex format
MeshData createColorCubeMesh(float size = 1.0f);

/// @brief Create a standard cube mesh with normals and UVs
/// @param size Size of the cube (side length)
/// @return MeshData with StandardVertex format
MeshData createCubeMesh(float size = 1.0f);

/// @brief Create a plane mesh
/// @param width Width of the plane
/// @param height Height of the plane
/// @return MeshData with StandardVertex format
MeshData createPlaneMesh(float width = 1.0f, float height = 1.0f);

/// @brief Create a sphere mesh
/// @param radius Radius of the sphere
/// @param segments Number of horizontal segments
/// @param rings Number of vertical rings
/// @return MeshData with StandardVertex format
MeshData createSphereMesh(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);

} // namespace vesper
