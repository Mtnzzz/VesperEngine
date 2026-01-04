#pragma once

#include "runtime/function/render/rhi/rhi_types.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace vesper {

class RHI;

/// Single resource binding info extracted from shader
struct ShaderResourceBinding
{
    std::string       name;
    uint32_t          set         = 0;
    uint32_t          binding     = 0;
    RHIDescriptorType type        = RHIDescriptorType::UniformBuffer;
    uint32_t          count       = 1;  // Array size (1 for non-arrays)
    RHIShaderStage    stageFlags  = RHIShaderStage::None;
};

/// Push constant range extracted from shader
struct ShaderPushConstantRange
{
    std::string     name;
    uint32_t        offset      = 0;
    uint32_t        size        = 0;
    RHIShaderStage  stageFlags  = RHIShaderStage::None;
};

/// Vertex input attribute extracted from vertex shader
struct ShaderVertexAttribute
{
    std::string name;
    uint32_t    location = 0;
    RHIFormat   format   = RHIFormat::Undefined;
    uint32_t    vecSize  = 0;  // 1, 2, 3, or 4
};

/// Complete reflection data for a shader program
struct ShaderProgramReflection
{
    std::vector<ShaderResourceBinding>    bindings;
    std::vector<ShaderPushConstantRange>  pushConstants;
    std::vector<ShaderVertexAttribute>    vertexInputs;

    // Grouped by set index for easy layout creation
    std::unordered_map<uint32_t, std::vector<ShaderResourceBinding>> bindingsBySet;

    // Total push constant size
    uint32_t totalPushConstantSize = 0;

    /// Find binding by name
    const ShaderResourceBinding* findBinding(const std::string& name) const;

    /// Find vertex attribute by name
    const ShaderVertexAttribute* findVertexAttribute(const std::string& name) const;

    /// Get maximum descriptor set index used
    uint32_t getMaxSetIndex() const;
};

/// Shader reflection utility using SPIRV-Cross
class ShaderReflector
{
public:
    /// Reflect a single SPIR-V shader stage
    /// @param spirvData  Raw SPIR-V bytecode (as uint32_t words)
    /// @param stage      Shader stage (Vertex, Fragment, etc.)
    /// @return Reflection data for this stage
    static ShaderProgramReflection reflectStage(
        const std::vector<uint32_t>& spirvData,
        RHIShaderStage stage
    );

    /// Merge reflections from multiple shader stages into one program
    /// Combines bindings with matching set/binding numbers
    static ShaderProgramReflection mergeStages(
        const std::vector<ShaderProgramReflection>& stages
    );

    /// Convenience: reflect vertex + fragment shaders from .spv files
    /// @param vertexSpvPath   Path to vertex shader SPIR-V
    /// @param fragmentSpvPath Path to fragment shader SPIR-V
    /// @return Combined reflection data
    static ShaderProgramReflection reflectProgram(
        const std::string& vertexSpvPath,
        const std::string& fragmentSpvPath
    );

    /// Create descriptor set layouts from reflection data
    /// Creates one layout per set index (0 to maxSetIndex)
    static std::vector<RHIDescriptorSetLayoutHandle> createDescriptorSetLayouts(
        RHI* rhi,
        const ShaderProgramReflection& reflection
    );

    /// Create vertex input state from reflection data
    /// Computes proper offsets and stride automatically
    static RHIVertexInputState createVertexInputState(
        const ShaderProgramReflection& reflection
    );

    /// Create push constant ranges from reflection data
    static std::vector<RHIPushConstantRange> createPushConstantRanges(
        const ShaderProgramReflection& reflection
    );

    /// Load SPIR-V file as uint32_t words
    static std::vector<uint32_t> loadSpirv(const std::string& path);

    /// Get size in bytes for a given RHIFormat
    static uint32_t getFormatSize(RHIFormat format);
};

} // namespace vesper
