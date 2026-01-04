// VesperEngine Minimal 3D Shader - Slang
// Validates: Push Constants (MVP matrix) for 3D rendering

struct VertexInput {
    float3 position : POSITION;
    float3 color    : COLOR0;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

// Push Constants for MVP transformation matrix
// Using row_major to match DirectXMath's row-major storage
struct PushConstants {
    row_major float4x4 mvp;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;
    // For row-major matrices from DirectXMath, use vector * matrix order
    output.position = mul(float4(input.position, 1.0), pushConstants.mvp);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
