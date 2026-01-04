#pragma once

#include "runtime/function/render/texture.h"
#include "runtime/function/render/rhi/rhi.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace vesper {

class TextureManager;

/// @brief PBR material texture slots
enum class MaterialTextureSlot : uint8_t
{
    Albedo = 0,         // Base color (sRGB)
    Normal = 1,         // Normal map (linear)
    Metallic = 2,       // Metallic (linear, single channel)
    Roughness = 3,      // Roughness (linear, single channel)
    AO = 4,             // Ambient occlusion (linear, single channel)
    Count = 5
};

/// @brief PBR material parameters (CPU side)
struct MaterialData
{
    // Base color multiplier (combined with albedo texture)
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};

    // PBR factors (multiplied with texture values)
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float aoFactor = 1.0f;
    float emissiveIntensity = 0.0f;

    // Emissive color
    glm::vec3 emissiveColor{0.0f, 0.0f, 0.0f};

    // Alpha mode
    float alphaCutoff = 0.5f;

    // Flags
    bool doubleSided = false;
    bool useAlphaBlend = false;

    // Texture paths (for serialization/loading)
    std::string albedoPath;
    std::string normalPath;
    std::string metallicPath;
    std::string roughnessPath;
    std::string aoPath;
};

/// @brief GPU material uniform buffer data (packed for shader)
struct alignas(16) MaterialUniformData
{
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissiveColor{0.0f, 0.0f, 0.0f, 1.0f};  // w = emissiveIntensity
    glm::vec4 pbrFactors{0.0f, 1.0f, 1.0f, 0.5f};     // x=metallic, y=roughness, z=ao, w=alphaCutoff
    glm::uvec4 textureFlags{0, 0, 0, 0};               // Bitflags for which textures are bound
};

/// @brief PBR material with textures and parameters
class Material
{
public:
    Material() = default;
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    /// @brief Create material from data and texture manager
    /// @param rhi RHI instance
    /// @param data Material parameters
    /// @param textureManager Texture manager for loading textures
    /// @return Shared pointer to material
    static std::shared_ptr<Material> create(
        RHI* rhi,
        const MaterialData& data,
        TextureManager* textureManager = nullptr,
        const char* debugName = nullptr
    );

    /// @brief Create material with explicit textures
    static std::shared_ptr<Material> create(
        RHI* rhi,
        const MaterialData& data,
        TexturePtr albedo,
        TexturePtr normal,
        TexturePtr metallic,
        TexturePtr roughness,
        TexturePtr ao,
        const char* debugName = nullptr
    );

    // =========================================================================
    // Texture Management
    // =========================================================================

    /// @brief Set texture for a slot
    void setTexture(MaterialTextureSlot slot, TexturePtr texture);

    /// @brief Get texture from a slot
    TexturePtr getTexture(MaterialTextureSlot slot) const;

    /// @brief Check if a texture slot is bound
    bool hasTexture(MaterialTextureSlot slot) const;

    // =========================================================================
    // Parameters
    // =========================================================================

    /// @brief Set base color multiplier
    void setBaseColor(const glm::vec4& color);
    glm::vec4 getBaseColor() const { return m_data.baseColor; }

    /// @brief Set metallic factor
    void setMetallicFactor(float factor);
    float getMetallicFactor() const { return m_data.metallicFactor; }

    /// @brief Set roughness factor
    void setRoughnessFactor(float factor);
    float getRoughnessFactor() const { return m_data.roughnessFactor; }

    /// @brief Set AO factor
    void setAOFactor(float factor);
    float getAOFactor() const { return m_data.aoFactor; }

    /// @brief Set emissive color
    void setEmissiveColor(const glm::vec3& color);
    glm::vec3 getEmissiveColor() const { return m_data.emissiveColor; }

    /// @brief Set emissive intensity
    void setEmissiveIntensity(float intensity);
    float getEmissiveIntensity() const { return m_data.emissiveIntensity; }

    /// @brief Set alpha cutoff threshold
    void setAlphaCutoff(float cutoff);
    float getAlphaCutoff() const { return m_data.alphaCutoff; }

    /// @brief Set double-sided flag
    void setDoubleSided(bool doubleSided);
    bool isDoubleSided() const { return m_data.doubleSided; }

    /// @brief Set alpha blend mode
    void setAlphaBlend(bool enabled);
    bool usesAlphaBlend() const { return m_data.useAlphaBlend; }

    // =========================================================================
    // GPU Resources
    // =========================================================================

    /// @brief Get uniform buffer for shader binding
    RHIBufferHandle getUniformBuffer() const { return m_uniformBuffer; }

    /// @brief Update uniform buffer with current parameters
    void updateUniformBuffer();

    /// @brief Create descriptor set for this material
    /// @param layout Descriptor set layout to use
    /// @return true if successful
    bool createDescriptorSet(RHIDescriptorSetLayoutHandle layout);

    /// @brief Get descriptor set for shader binding
    RHIDescriptorSetHandle getDescriptorSet() const { return m_descriptorSet; }

    /// @brief Check if descriptor set is created
    bool hasDescriptorSet() const { return m_descriptorSet != nullptr; }

    /// @brief Get material data (for serialization)
    const MaterialData& getData() const { return m_data; }

    /// @brief Check validity
    bool isValid() const { return m_uniformBuffer != nullptr; }

    /// @brief Get debug name
    const std::string& getName() const { return m_name; }

private:
    /// @brief Initialize GPU resources
    bool initializeGPUResources(RHI* rhi);

    /// @brief Build texture flags for shader
    uint32_t buildTextureFlags() const;

private:
    RHI* m_rhi = nullptr;
    std::string m_name;

    // Material data
    MaterialData m_data;

    // Textures (indexed by MaterialTextureSlot)
    TexturePtr m_textures[static_cast<size_t>(MaterialTextureSlot::Count)];

    // GPU uniform buffer
    RHIBufferHandle m_uniformBuffer;

    // Descriptor set for textures
    RHIDescriptorSetHandle m_descriptorSet;

    // Dirty flag for uniform buffer update
    bool m_uniformDirty = true;
};

using MaterialPtr = std::shared_ptr<Material>;

} // namespace vesper
