#include "runtime/function/render/shader_reflector.h"
#include "runtime/function/render/rhi/rhi.h"
#include "runtime/core/log/log_system.h"

#include <spirv_cross.hpp>

#include <fstream>
#include <algorithm>

namespace vesper {

// ============================================================================
// ShaderProgramReflection Implementation
// ============================================================================

const ShaderResourceBinding* ShaderProgramReflection::findBinding(const std::string& name) const
{
    for (const auto& binding : bindings)
    {
        if (binding.name == name)
        {
            return &binding;
        }
    }
    return nullptr;
}

const ShaderVertexAttribute* ShaderProgramReflection::findVertexAttribute(const std::string& name) const
{
    for (const auto& attr : vertexInputs)
    {
        if (attr.name == name)
        {
            return &attr;
        }
    }
    return nullptr;
}

uint32_t ShaderProgramReflection::getMaxSetIndex() const
{
    uint32_t maxSet = 0;
    for (const auto& [setIndex, _] : bindingsBySet)
    {
        maxSet = std::max(maxSet, setIndex);
    }
    return maxSet;
}

// ============================================================================
// Helper Functions
// ============================================================================

static RHIFormat spirvTypeToRHIFormat(const spirv_cross::SPIRType& type)
{
    if (type.basetype == spirv_cross::SPIRType::Float)
    {
        switch (type.vecsize)
        {
            case 1: return RHIFormat::R32_FLOAT;
            case 2: return RHIFormat::RG32_FLOAT;
            case 3: return RHIFormat::RGB32_FLOAT;
            case 4: return RHIFormat::RGBA32_FLOAT;
        }
    }
    else if (type.basetype == spirv_cross::SPIRType::Int)
    {
        switch (type.vecsize)
        {
            case 1: return RHIFormat::R32_SINT;
            case 2: return RHIFormat::RG32_SINT;
            case 3: return RHIFormat::RGB32_SINT;
            case 4: return RHIFormat::RGBA32_SINT;
        }
    }
    else if (type.basetype == spirv_cross::SPIRType::UInt)
    {
        switch (type.vecsize)
        {
            case 1: return RHIFormat::R32_UINT;
            case 2: return RHIFormat::RG32_UINT;
            case 3: return RHIFormat::RGB32_UINT;
            case 4: return RHIFormat::RGBA32_UINT;
        }
    }

    return RHIFormat::Undefined;
}

// ============================================================================
// ShaderReflector Implementation
// ============================================================================

std::vector<uint32_t> ShaderReflector::loadSpirv(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOG_ERROR("ShaderReflector", "Failed to open SPIR-V file: {}", path);
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % sizeof(uint32_t) != 0)
    {
        LOG_ERROR("ShaderReflector", "Invalid SPIR-V file size: {}", path);
        return {};
    }

    std::vector<uint32_t> spirvData(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirvData.data()), fileSize);

    return spirvData;
}

uint32_t ShaderReflector::getFormatSize(RHIFormat format)
{
    switch (format)
    {
        case RHIFormat::R8_UNORM:
        case RHIFormat::R8_SNORM:
        case RHIFormat::R8_UINT:
        case RHIFormat::R8_SINT:
            return 1;

        case RHIFormat::RG8_UNORM:
        case RHIFormat::RG8_SNORM:
        case RHIFormat::RG8_UINT:
        case RHIFormat::RG8_SINT:
        case RHIFormat::R16_UNORM:
        case RHIFormat::R16_SNORM:
        case RHIFormat::R16_UINT:
        case RHIFormat::R16_SINT:
        case RHIFormat::R16_FLOAT:
            return 2;

        case RHIFormat::RGBA8_UNORM:
        case RHIFormat::RGBA8_SNORM:
        case RHIFormat::RGBA8_UINT:
        case RHIFormat::RGBA8_SINT:
        case RHIFormat::RGBA8_SRGB:
        case RHIFormat::BGRA8_UNORM:
        case RHIFormat::BGRA8_SRGB:
        case RHIFormat::RG16_UNORM:
        case RHIFormat::RG16_SNORM:
        case RHIFormat::RG16_UINT:
        case RHIFormat::RG16_SINT:
        case RHIFormat::RG16_FLOAT:
        case RHIFormat::R32_UINT:
        case RHIFormat::R32_SINT:
        case RHIFormat::R32_FLOAT:
            return 4;

        case RHIFormat::RGBA16_UNORM:
        case RHIFormat::RGBA16_SNORM:
        case RHIFormat::RGBA16_UINT:
        case RHIFormat::RGBA16_SINT:
        case RHIFormat::RGBA16_FLOAT:
        case RHIFormat::RG32_UINT:
        case RHIFormat::RG32_SINT:
        case RHIFormat::RG32_FLOAT:
            return 8;

        case RHIFormat::RGB32_UINT:
        case RHIFormat::RGB32_SINT:
        case RHIFormat::RGB32_FLOAT:
            return 12;

        case RHIFormat::RGBA32_UINT:
        case RHIFormat::RGBA32_SINT:
        case RHIFormat::RGBA32_FLOAT:
            return 16;

        default:
            return 0;
    }
}

ShaderProgramReflection ShaderReflector::reflectStage(
    const std::vector<uint32_t>& spirvData,
    RHIShaderStage stage)
{
    ShaderProgramReflection result;

    if (spirvData.empty())
    {
        LOG_ERROR("ShaderReflector", "Empty SPIR-V data provided");
        return result;
    }

    try
    {
        spirv_cross::Compiler compiler(spirvData);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        // ====================================================================
        // Reflect Uniform Buffers
        // ====================================================================
        for (const auto& ub : resources.uniform_buffers)
        {
            ShaderResourceBinding binding;
            binding.name = compiler.get_name(ub.id);
            if (binding.name.empty())
            {
                binding.name = compiler.get_fallback_name(ub.id);
            }
            binding.set = compiler.get_decoration(ub.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(ub.id, spv::DecorationBinding);
            binding.type = RHIDescriptorType::UniformBuffer;
            binding.count = 1;
            binding.stageFlags = stage;

            // Check for array
            const auto& type = compiler.get_type(ub.type_id);
            if (!type.array.empty())
            {
                binding.count = type.array[0];
            }

            result.bindings.push_back(binding);
            result.bindingsBySet[binding.set].push_back(binding);
        }

        // ====================================================================
        // Reflect Storage Buffers
        // ====================================================================
        for (const auto& sb : resources.storage_buffers)
        {
            ShaderResourceBinding binding;
            binding.name = compiler.get_name(sb.id);
            if (binding.name.empty())
            {
                binding.name = compiler.get_fallback_name(sb.id);
            }
            binding.set = compiler.get_decoration(sb.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(sb.id, spv::DecorationBinding);
            binding.type = RHIDescriptorType::StorageBuffer;
            binding.count = 1;
            binding.stageFlags = stage;

            result.bindings.push_back(binding);
            result.bindingsBySet[binding.set].push_back(binding);
        }

        // ====================================================================
        // Reflect Sampled Images (Combined Image Samplers)
        // ====================================================================
        for (const auto& img : resources.sampled_images)
        {
            ShaderResourceBinding binding;
            binding.name = compiler.get_name(img.id);
            if (binding.name.empty())
            {
                binding.name = compiler.get_fallback_name(img.id);
            }
            binding.set = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
            binding.type = RHIDescriptorType::CombinedImageSampler;
            binding.count = 1;
            binding.stageFlags = stage;

            // Check for texture array
            const auto& type = compiler.get_type(img.type_id);
            if (!type.array.empty())
            {
                binding.count = type.array[0];
            }

            result.bindings.push_back(binding);
            result.bindingsBySet[binding.set].push_back(binding);
        }

        // ====================================================================
        // Reflect Separate Images
        // ====================================================================
        for (const auto& img : resources.separate_images)
        {
            ShaderResourceBinding binding;
            binding.name = compiler.get_name(img.id);
            if (binding.name.empty())
            {
                binding.name = compiler.get_fallback_name(img.id);
            }
            binding.set = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
            binding.type = RHIDescriptorType::SampledImage;
            binding.count = 1;
            binding.stageFlags = stage;

            result.bindings.push_back(binding);
            result.bindingsBySet[binding.set].push_back(binding);
        }

        // ====================================================================
        // Reflect Separate Samplers
        // ====================================================================
        for (const auto& sampler : resources.separate_samplers)
        {
            ShaderResourceBinding binding;
            binding.name = compiler.get_name(sampler.id);
            if (binding.name.empty())
            {
                binding.name = compiler.get_fallback_name(sampler.id);
            }
            binding.set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
            binding.type = RHIDescriptorType::Sampler;
            binding.count = 1;
            binding.stageFlags = stage;

            result.bindings.push_back(binding);
            result.bindingsBySet[binding.set].push_back(binding);
        }

        // ====================================================================
        // Reflect Storage Images
        // ====================================================================
        for (const auto& img : resources.storage_images)
        {
            ShaderResourceBinding binding;
            binding.name = compiler.get_name(img.id);
            if (binding.name.empty())
            {
                binding.name = compiler.get_fallback_name(img.id);
            }
            binding.set = compiler.get_decoration(img.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(img.id, spv::DecorationBinding);
            binding.type = RHIDescriptorType::StorageImage;
            binding.count = 1;
            binding.stageFlags = stage;

            result.bindings.push_back(binding);
            result.bindingsBySet[binding.set].push_back(binding);
        }

        // ====================================================================
        // Reflect Push Constants
        // ====================================================================
        for (const auto& pc : resources.push_constant_buffers)
        {
            const auto& type = compiler.get_type(pc.base_type_id);
            size_t pcSize = compiler.get_declared_struct_size(type);

            ShaderPushConstantRange range;
            range.name = compiler.get_name(pc.id);
            if (range.name.empty())
            {
                range.name = "PushConstants";
            }
            range.offset = 0;
            range.size = static_cast<uint32_t>(pcSize);
            range.stageFlags = stage;

            result.pushConstants.push_back(range);
            result.totalPushConstantSize = std::max(result.totalPushConstantSize, range.size);
        }

        // ====================================================================
        // Reflect Vertex Inputs (only for vertex shaders)
        // ====================================================================
        if (stage == RHIShaderStage::Vertex)
        {
            for (const auto& input : resources.stage_inputs)
            {
                ShaderVertexAttribute attr;
                attr.name = compiler.get_name(input.id);
                if (attr.name.empty())
                {
                    attr.name = compiler.get_fallback_name(input.id);
                }
                attr.location = compiler.get_decoration(input.id, spv::DecorationLocation);

                const auto& type = compiler.get_type(input.type_id);
                attr.format = spirvTypeToRHIFormat(type);
                attr.vecSize = type.vecsize;

                result.vertexInputs.push_back(attr);
            }

            // Sort by location
            std::sort(result.vertexInputs.begin(), result.vertexInputs.end(),
                [](const auto& a, const auto& b) { return a.location < b.location; });
        }

        LOG_INFO("ShaderReflector", "Reflected shader: {} bindings, {} push constants, {} vertex inputs",
            result.bindings.size(), result.pushConstants.size(), result.vertexInputs.size());
    }
    catch (const spirv_cross::CompilerError& e)
    {
        LOG_ERROR("ShaderReflector", "SPIRV-Cross error: {}", e.what());
    }

    return result;
}

ShaderProgramReflection ShaderReflector::mergeStages(
    const std::vector<ShaderProgramReflection>& stages)
{
    ShaderProgramReflection result;

    for (const auto& stage : stages)
    {
        // Merge bindings
        for (const auto& binding : stage.bindings)
        {
            // Check if binding already exists
            bool found = false;
            for (auto& existing : result.bindings)
            {
                if (existing.set == binding.set && existing.binding == binding.binding)
                {
                    // Merge stage flags
                    existing.stageFlags = existing.stageFlags | binding.stageFlags;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                result.bindings.push_back(binding);
            }
        }

        // Merge push constants
        for (const auto& pc : stage.pushConstants)
        {
            bool found = false;
            for (auto& existing : result.pushConstants)
            {
                if (existing.offset == pc.offset && existing.size == pc.size)
                {
                    existing.stageFlags = existing.stageFlags | pc.stageFlags;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                result.pushConstants.push_back(pc);
            }
        }

        // Only take vertex inputs from vertex shader
        if (result.vertexInputs.empty() && !stage.vertexInputs.empty())
        {
            result.vertexInputs = stage.vertexInputs;
        }

        // Update total push constant size
        result.totalPushConstantSize = std::max(result.totalPushConstantSize, stage.totalPushConstantSize);
    }

    // Rebuild bindingsBySet
    result.bindingsBySet.clear();
    for (const auto& binding : result.bindings)
    {
        result.bindingsBySet[binding.set].push_back(binding);
    }

    return result;
}

ShaderProgramReflection ShaderReflector::reflectProgram(
    const std::string& vertexSpvPath,
    const std::string& fragmentSpvPath)
{
    auto vertexSpirv = loadSpirv(vertexSpvPath);
    auto fragmentSpirv = loadSpirv(fragmentSpvPath);

    if (vertexSpirv.empty() || fragmentSpirv.empty())
    {
        LOG_ERROR("ShaderReflector", "Failed to load shader files");
        return {};
    }

    auto vertexReflection = reflectStage(vertexSpirv, RHIShaderStage::Vertex);
    auto fragmentReflection = reflectStage(fragmentSpirv, RHIShaderStage::Fragment);

    return mergeStages({vertexReflection, fragmentReflection});
}

std::vector<RHIDescriptorSetLayoutHandle> ShaderReflector::createDescriptorSetLayouts(
    RHI* rhi,
    const ShaderProgramReflection& reflection)
{
    std::vector<RHIDescriptorSetLayoutHandle> layouts;

    if (reflection.bindingsBySet.empty())
    {
        return layouts;
    }

    uint32_t maxSet = reflection.getMaxSetIndex();

    for (uint32_t set = 0; set <= maxSet; ++set)
    {
        RHIDescriptorSetLayoutDesc desc;

        auto it = reflection.bindingsBySet.find(set);
        if (it != reflection.bindingsBySet.end())
        {
            for (const auto& binding : it->second)
            {
                RHIDescriptorBinding layoutBinding;
                layoutBinding.binding = binding.binding;
                layoutBinding.descriptorType = binding.type;
                layoutBinding.descriptorCount = binding.count;
                layoutBinding.stageFlags = binding.stageFlags;
                desc.bindings.push_back(layoutBinding);
            }
        }

        auto layout = rhi->createDescriptorSetLayout(desc);
        layouts.push_back(layout);

        LOG_DEBUG("ShaderReflector", "Created descriptor set layout for set {} with {} bindings",
            set, desc.bindings.size());
    }

    return layouts;
}

RHIVertexInputState ShaderReflector::createVertexInputState(
    const ShaderProgramReflection& reflection)
{
    RHIVertexInputState state;

    if (reflection.vertexInputs.empty())
    {
        return state;
    }

    // Calculate stride and offsets
    uint32_t offset = 0;
    for (const auto& attr : reflection.vertexInputs)
    {
        RHIVertexAttribute rhiAttr;
        rhiAttr.location = attr.location;
        rhiAttr.binding = 0;  // Single binding for now
        rhiAttr.format = attr.format;
        rhiAttr.offset = offset;

        offset += getFormatSize(attr.format);
        state.attributes.push_back(rhiAttr);
    }

    // Create single vertex binding
    RHIVertexBinding binding;
    binding.binding = 0;
    binding.stride = offset;
    binding.inputRate = RHIVertexInputRate::Vertex;
    state.bindings.push_back(binding);

    LOG_DEBUG("ShaderReflector", "Created vertex input state with {} attributes, stride={}",
        state.attributes.size(), offset);

    return state;
}

std::vector<RHIPushConstantRange> ShaderReflector::createPushConstantRanges(
    const ShaderProgramReflection& reflection)
{
    std::vector<RHIPushConstantRange> ranges;

    for (const auto& pc : reflection.pushConstants)
    {
        RHIPushConstantRange range;
        range.stages = pc.stageFlags;
        range.offset = pc.offset;
        range.size = pc.size;
        ranges.push_back(range);
    }

    return ranges;
}

} // namespace vesper
