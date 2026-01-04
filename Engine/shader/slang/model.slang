// VesperEngine Model Shader - Slang
// Supports: Textured models with MVP transformation

struct VertexInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent  : TANGENT;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// Push Constants for transformation matrices
// Using row_major to match DirectXMath's row-major storage
struct PushConstants {
    row_major float4x4 mvp;
    row_major float4x4 model;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

// Texture and sampler (set 0, binding 0)
[[vk::binding(0, 0)]]
Texture2D<float4> albedoTexture;

[[vk::binding(1, 0)]]
SamplerState albedoSampler;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;
    // For row-major matrices from DirectXMath, use vector * matrix order
    output.position = mul(float4(input.position, 1.0), pushConstants.mvp);
    output.normal = mul(input.normal, (float3x3)pushConstants.model);
    output.texCoord = input.texCoord;
    output.worldPos = mul(float4(input.position, 1.0), pushConstants.model).xyz;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    // Sample albedo texture
    float4 albedo = albedoTexture.Sample(albedoSampler, input.texCoord);

    // Simple directional lighting
    float3 lightDir = normalize(float3(1.0, 1.0, -1.0));
    float3 normal = normalize(input.normal);
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Ambient + diffuse lighting
    float3 ambient = 0.3;
    float3 diffuse = NdotL * 0.7;

    float3 finalColor = albedo.rgb * (ambient + diffuse);
    return float4(finalColor, albedo.a);
}
