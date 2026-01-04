#include "mesh_primitives.h"

#include <cmath>

namespace vesper {

MeshData createColorCubeMesh(float size)
{
    MeshData data;

    const float halfSize = size * 0.5f;

    // 8 unique vertices with distinct colors for debugging
    // Each corner has a different color based on its position
    std::vector<ColorVertex> vertices = {
        // Front face (z = +halfSize)
        {{ -halfSize, -halfSize,  halfSize }, { 0.0f, 0.0f, 0.0f }},  // 0: black
        {{  halfSize, -halfSize,  halfSize }, { 1.0f, 0.0f, 0.0f }},  // 1: red
        {{  halfSize,  halfSize,  halfSize }, { 1.0f, 1.0f, 0.0f }},  // 2: yellow
        {{ -halfSize,  halfSize,  halfSize }, { 0.0f, 1.0f, 0.0f }},  // 3: green

        // Back face (z = -halfSize)
        {{ -halfSize, -halfSize, -halfSize }, { 0.0f, 0.0f, 1.0f }},  // 4: blue
        {{  halfSize, -halfSize, -halfSize }, { 1.0f, 0.0f, 1.0f }},  // 5: magenta
        {{  halfSize,  halfSize, -halfSize }, { 1.0f, 1.0f, 1.0f }},  // 6: white
        {{ -halfSize,  halfSize, -halfSize }, { 0.0f, 1.0f, 1.0f }},  // 7: cyan
    };

    // 36 indices for 12 triangles (6 faces * 2 triangles)
    data.indices = {
        // Front face
        0, 1, 2,  2, 3, 0,
        // Back face
        5, 4, 7,  7, 6, 5,
        // Left face
        4, 0, 3,  3, 7, 4,
        // Right face
        1, 5, 6,  6, 2, 1,
        // Top face
        3, 2, 6,  6, 7, 3,
        // Bottom face
        4, 5, 1,  1, 0, 4,
    };

    data.setVertices(vertices);
    data.vertexLayout = ColorVertex::getVertexLayout();

    return data;
}

MeshData createCubeMesh(float size)
{
    MeshData data;

    const float halfSize = size * 0.5f;

    // 24 vertices (6 faces * 4 vertices) for proper normals
    std::vector<StandardVertex> vertices;
    vertices.reserve(24);

    // Front face (z = +halfSize, normal = +Z)
    vertices.push_back({{ -halfSize, -halfSize,  halfSize }, { 0, 0, 1 }, { 0, 1 }});
    vertices.push_back({{  halfSize, -halfSize,  halfSize }, { 0, 0, 1 }, { 1, 1 }});
    vertices.push_back({{  halfSize,  halfSize,  halfSize }, { 0, 0, 1 }, { 1, 0 }});
    vertices.push_back({{ -halfSize,  halfSize,  halfSize }, { 0, 0, 1 }, { 0, 0 }});

    // Back face (z = -halfSize, normal = -Z)
    vertices.push_back({{  halfSize, -halfSize, -halfSize }, { 0, 0, -1 }, { 0, 1 }});
    vertices.push_back({{ -halfSize, -halfSize, -halfSize }, { 0, 0, -1 }, { 1, 1 }});
    vertices.push_back({{ -halfSize,  halfSize, -halfSize }, { 0, 0, -1 }, { 1, 0 }});
    vertices.push_back({{  halfSize,  halfSize, -halfSize }, { 0, 0, -1 }, { 0, 0 }});

    // Right face (x = +halfSize, normal = +X)
    vertices.push_back({{  halfSize, -halfSize,  halfSize }, { 1, 0, 0 }, { 0, 1 }});
    vertices.push_back({{  halfSize, -halfSize, -halfSize }, { 1, 0, 0 }, { 1, 1 }});
    vertices.push_back({{  halfSize,  halfSize, -halfSize }, { 1, 0, 0 }, { 1, 0 }});
    vertices.push_back({{  halfSize,  halfSize,  halfSize }, { 1, 0, 0 }, { 0, 0 }});

    // Left face (x = -halfSize, normal = -X)
    vertices.push_back({{ -halfSize, -halfSize, -halfSize }, { -1, 0, 0 }, { 0, 1 }});
    vertices.push_back({{ -halfSize, -halfSize,  halfSize }, { -1, 0, 0 }, { 1, 1 }});
    vertices.push_back({{ -halfSize,  halfSize,  halfSize }, { -1, 0, 0 }, { 1, 0 }});
    vertices.push_back({{ -halfSize,  halfSize, -halfSize }, { -1, 0, 0 }, { 0, 0 }});

    // Top face (y = +halfSize, normal = +Y)
    vertices.push_back({{ -halfSize,  halfSize,  halfSize }, { 0, 1, 0 }, { 0, 1 }});
    vertices.push_back({{  halfSize,  halfSize,  halfSize }, { 0, 1, 0 }, { 1, 1 }});
    vertices.push_back({{  halfSize,  halfSize, -halfSize }, { 0, 1, 0 }, { 1, 0 }});
    vertices.push_back({{ -halfSize,  halfSize, -halfSize }, { 0, 1, 0 }, { 0, 0 }});

    // Bottom face (y = -halfSize, normal = -Y)
    vertices.push_back({{ -halfSize, -halfSize, -halfSize }, { 0, -1, 0 }, { 0, 1 }});
    vertices.push_back({{  halfSize, -halfSize, -halfSize }, { 0, -1, 0 }, { 1, 1 }});
    vertices.push_back({{  halfSize, -halfSize,  halfSize }, { 0, -1, 0 }, { 1, 0 }});
    vertices.push_back({{ -halfSize, -halfSize,  halfSize }, { 0, -1, 0 }, { 0, 0 }});

    // 36 indices for 12 triangles
    data.indices = {
        0,  1,  2,   2,  3,  0,   // Front
        4,  5,  6,   6,  7,  4,   // Back
        8,  9,  10,  10, 11, 8,   // Right
        12, 13, 14,  14, 15, 12,  // Left
        16, 17, 18,  18, 19, 16,  // Top
        20, 21, 22,  22, 23, 20,  // Bottom
    };

    data.setVertices(vertices);
    data.vertexLayout = StandardVertex::getVertexLayout();

    return data;
}

MeshData createPlaneMesh(float width, float height)
{
    MeshData data;

    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;

    // 4 vertices for a quad
    std::vector<StandardVertex> vertices = {
        {{ -halfW, 0.0f, -halfH }, { 0, 1, 0 }, { 0, 0 }},  // bottom-left
        {{  halfW, 0.0f, -halfH }, { 0, 1, 0 }, { 1, 0 }},  // bottom-right
        {{  halfW, 0.0f,  halfH }, { 0, 1, 0 }, { 1, 1 }},  // top-right
        {{ -halfW, 0.0f,  halfH }, { 0, 1, 0 }, { 0, 1 }},  // top-left
    };

    data.indices = {
        0, 1, 2,
        2, 3, 0,
    };

    data.setVertices(vertices);
    data.vertexLayout = StandardVertex::getVertexLayout();

    return data;
}

MeshData createSphereMesh(float radius, uint32_t segments, uint32_t rings)
{
    MeshData data;

    std::vector<StandardVertex> vertices;
    vertices.reserve((rings + 1) * (segments + 1));

    const float PI = 3.14159265358979323846f;

    for (uint32_t y = 0; y <= rings; ++y)
    {
        for (uint32_t x = 0; x <= segments; ++x)
        {
            float xSegment = static_cast<float>(x) / static_cast<float>(segments);
            float ySegment = static_cast<float>(y) / static_cast<float>(rings);

            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            StandardVertex vertex;
            vertex.position = Vector3(xPos * radius, yPos * radius, zPos * radius);
            vertex.normal = Vector3(xPos, yPos, zPos);  // Normalized since xPos^2 + yPos^2 + zPos^2 = 1
            vertex.texCoord = Vector2(xSegment, ySegment);
            vertices.push_back(vertex);
        }
    }

    // Generate indices
    data.indices.reserve(rings * segments * 6);

    for (uint32_t y = 0; y < rings; ++y)
    {
        for (uint32_t x = 0; x < segments; ++x)
        {
            uint32_t current = y * (segments + 1) + x;
            uint32_t next = current + segments + 1;

            data.indices.push_back(current);
            data.indices.push_back(next);
            data.indices.push_back(current + 1);

            data.indices.push_back(current + 1);
            data.indices.push_back(next);
            data.indices.push_back(next + 1);
        }
    }

    data.setVertices(vertices);
    data.vertexLayout = StandardVertex::getVertexLayout();

    return data;
}

} // namespace vesper
