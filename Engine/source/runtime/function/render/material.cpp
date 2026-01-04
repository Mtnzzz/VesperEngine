#include "material.h"
#include "texture_manager.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

Material::~Material()
{
    if (m_rhi && m_uniformBuffer)
    {
        m_rhi->destroyBuffer(m_uniformBuffer);
    }
}

std::shared_ptr<Material> Material::create(
    RHI* rhi,
    const MaterialData& data,
    TextureManager* textureManager,
    const char* debugName)
{
    if (!rhi)
    {
        LOG_ERROR("Material::create: RHI is null");
        return nullptr;
    }

    auto material = std::make_shared<Material>();
    material->m_rhi = rhi;
    material->m_data = data;
    material->m_name = debugName ? debugName : "UnnamedMaterial";

    // Initialize GPU resources
    if (!material->initializeGPUResources(rhi))
    {
        LOG_ERROR("Material::create: Failed to initialize GPU resources");
        return nullptr;
    }

    // Load textures from paths if texture manager provided
    if (textureManager)
    {
        if (!data.albedoPath.empty())
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Albedo)] =
                textureManager->loadTextureSync(data.albedoPath, true);
        }
        if (!data.normalPath.empty())
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Normal)] =
                textureManager->loadTextureSync(data.normalPath, false);
        }
        if (!data.metallicPath.empty())
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Metallic)] =
                textureManager->loadTextureSync(data.metallicPath, false);
        }
        if (!data.roughnessPath.empty())
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Roughness)] =
                textureManager->loadTextureSync(data.roughnessPath, false);
        }
        if (!data.aoPath.empty())
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::AO)] =
                textureManager->loadTextureSync(data.aoPath, false);
        }

        // Use default textures for missing slots
        if (!material->hasTexture(MaterialTextureSlot::Albedo))
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Albedo)] =
                textureManager->getDefaultWhite();
        }
        if (!material->hasTexture(MaterialTextureSlot::Normal))
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Normal)] =
                textureManager->getDefaultNormal();
        }
        if (!material->hasTexture(MaterialTextureSlot::Metallic))
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Metallic)] =
                textureManager->getDefaultBlack();
        }
        if (!material->hasTexture(MaterialTextureSlot::Roughness))
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::Roughness)] =
                textureManager->getDefaultWhite();
        }
        if (!material->hasTexture(MaterialTextureSlot::AO))
        {
            material->m_textures[static_cast<size_t>(MaterialTextureSlot::AO)] =
                textureManager->getDefaultWhite();
        }
    }

    // Initial uniform buffer update
    material->updateUniformBuffer();

    LOG_DEBUG("Material::create: Created '{}'", material->m_name);
    return material;
}

std::shared_ptr<Material> Material::create(
    RHI* rhi,
    const MaterialData& data,
    TexturePtr albedo,
    TexturePtr normal,
    TexturePtr metallic,
    TexturePtr roughness,
    TexturePtr ao,
    const char* debugName)
{
    if (!rhi)
    {
        LOG_ERROR("Material::create: RHI is null");
        return nullptr;
    }

    auto material = std::make_shared<Material>();
    material->m_rhi = rhi;
    material->m_data = data;
    material->m_name = debugName ? debugName : "UnnamedMaterial";

    // Initialize GPU resources
    if (!material->initializeGPUResources(rhi))
    {
        LOG_ERROR("Material::create: Failed to initialize GPU resources");
        return nullptr;
    }

    // Set textures directly
    material->m_textures[static_cast<size_t>(MaterialTextureSlot::Albedo)] = albedo;
    material->m_textures[static_cast<size_t>(MaterialTextureSlot::Normal)] = normal;
    material->m_textures[static_cast<size_t>(MaterialTextureSlot::Metallic)] = metallic;
    material->m_textures[static_cast<size_t>(MaterialTextureSlot::Roughness)] = roughness;
    material->m_textures[static_cast<size_t>(MaterialTextureSlot::AO)] = ao;

    // Initial uniform buffer update
    material->updateUniformBuffer();

    LOG_DEBUG("Material::create: Created '{}' with explicit textures", material->m_name);
    return material;
}

bool Material::initializeGPUResources(RHI* rhi)
{
    // Create uniform buffer
    RHIBufferDesc bufferDesc{};
    bufferDesc.size = sizeof(MaterialUniformData);
    bufferDesc.usage = RHIBufferUsage::Uniform;
    bufferDesc.memoryUsage = RHIMemoryUsage::CpuToGpu;
    bufferDesc.debugName = (m_name + "_UBO").c_str();

    m_uniformBuffer = rhi->createBuffer(bufferDesc);
    if (!m_uniformBuffer)
    {
        LOG_ERROR("Material: Failed to create uniform buffer");
        return false;
    }

    return true;
}

// =============================================================================
// Texture Management
// =============================================================================

void Material::setTexture(MaterialTextureSlot slot, TexturePtr texture)
{
    size_t index = static_cast<size_t>(slot);
    if (index < static_cast<size_t>(MaterialTextureSlot::Count))
    {
        m_textures[index] = texture;
        m_uniformDirty = true;
    }
}

TexturePtr Material::getTexture(MaterialTextureSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index < static_cast<size_t>(MaterialTextureSlot::Count))
    {
        return m_textures[index];
    }
    return nullptr;
}

bool Material::hasTexture(MaterialTextureSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index < static_cast<size_t>(MaterialTextureSlot::Count))
    {
        return m_textures[index] != nullptr && m_textures[index]->isValid();
    }
    return false;
}

// =============================================================================
// Parameters
// =============================================================================

void Material::setBaseColor(const glm::vec4& color)
{
    m_data.baseColor = color;
    m_uniformDirty = true;
}

void Material::setMetallicFactor(float factor)
{
    m_data.metallicFactor = glm::clamp(factor, 0.0f, 1.0f);
    m_uniformDirty = true;
}

void Material::setRoughnessFactor(float factor)
{
    m_data.roughnessFactor = glm::clamp(factor, 0.0f, 1.0f);
    m_uniformDirty = true;
}

void Material::setAOFactor(float factor)
{
    m_data.aoFactor = glm::clamp(factor, 0.0f, 1.0f);
    m_uniformDirty = true;
}

void Material::setEmissiveColor(const glm::vec3& color)
{
    m_data.emissiveColor = color;
    m_uniformDirty = true;
}

void Material::setEmissiveIntensity(float intensity)
{
    m_data.emissiveIntensity = glm::max(0.0f, intensity);
    m_uniformDirty = true;
}

void Material::setAlphaCutoff(float cutoff)
{
    m_data.alphaCutoff = glm::clamp(cutoff, 0.0f, 1.0f);
    m_uniformDirty = true;
}

void Material::setDoubleSided(bool doubleSided)
{
    m_data.doubleSided = doubleSided;
}

void Material::setAlphaBlend(bool enabled)
{
    m_data.useAlphaBlend = enabled;
}

// =============================================================================
// GPU Resources
// =============================================================================

void Material::updateUniformBuffer()
{
    if (!m_rhi || !m_uniformBuffer)
    {
        return;
    }

    MaterialUniformData uniformData;
    uniformData.baseColor = m_data.baseColor;
    uniformData.emissiveColor = glm::vec4(m_data.emissiveColor, m_data.emissiveIntensity);
    uniformData.pbrFactors = glm::vec4(
        m_data.metallicFactor,
        m_data.roughnessFactor,
        m_data.aoFactor,
        m_data.alphaCutoff
    );
    uniformData.textureFlags = glm::uvec4(buildTextureFlags(), 0, 0, 0);

    m_rhi->updateBuffer(m_uniformBuffer, &uniformData, sizeof(uniformData), 0);
    m_uniformDirty = false;
}

uint32_t Material::buildTextureFlags() const
{
    uint32_t flags = 0;

    if (hasTexture(MaterialTextureSlot::Albedo))
        flags |= (1u << static_cast<uint32_t>(MaterialTextureSlot::Albedo));
    if (hasTexture(MaterialTextureSlot::Normal))
        flags |= (1u << static_cast<uint32_t>(MaterialTextureSlot::Normal));
    if (hasTexture(MaterialTextureSlot::Metallic))
        flags |= (1u << static_cast<uint32_t>(MaterialTextureSlot::Metallic));
    if (hasTexture(MaterialTextureSlot::Roughness))
        flags |= (1u << static_cast<uint32_t>(MaterialTextureSlot::Roughness));
    if (hasTexture(MaterialTextureSlot::AO))
        flags |= (1u << static_cast<uint32_t>(MaterialTextureSlot::AO));

    return flags;
}

bool Material::createDescriptorSet(RHIDescriptorSetLayoutHandle layout)
{
    if (!m_rhi || !layout)
    {
        LOG_ERROR("Material::createDescriptorSet: Invalid RHI or layout");
        return false;
    }

    // Create descriptor set
    m_descriptorSet = m_rhi->createDescriptorSet(layout);
    if (!m_descriptorSet)
    {
        LOG_ERROR("Material::createDescriptorSet: Failed to create descriptor set for '{}'", m_name);
        return false;
    }

    // Get albedo texture (use default white if not available)
    TexturePtr albedo = m_textures[static_cast<size_t>(MaterialTextureSlot::Albedo)];
    if (!albedo || !albedo->isValid())
    {
        LOG_WARN("Material::createDescriptorSet: No albedo texture for '{}', using default", m_name);
        return false;
    }

    // Update descriptor set with albedo texture
    std::vector<RHIDescriptorWrite> writes;

    RHIDescriptorWrite texWrite{};
    texWrite.binding = 0;
    texWrite.type = RHIDescriptorType::SampledImage;
    texWrite.texture = albedo->getTexture();
    writes.push_back(texWrite);

    RHIDescriptorWrite samplerWrite{};
    samplerWrite.binding = 1;
    samplerWrite.type = RHIDescriptorType::Sampler;
    samplerWrite.sampler = albedo->getSampler();
    writes.push_back(samplerWrite);

    m_rhi->updateDescriptorSet(m_descriptorSet, writes);

    return true;
}

} // namespace vesper
