#pragma once

#include "runtime/function/render/rhi/rhi.h"
#include "runtime/function/render/rhi/rhi_types.h"

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace vesper {

/// @brief Loading state for async resources
enum class ResourceLoadState : uint8_t
{
    NotLoaded,      // Initial state
    Loading,        // Currently being loaded (CPU)
    Uploading,      // Data loaded, GPU upload pending
    Ready,          // Fully loaded and ready to use
    Failed          // Loading failed
};

/// @brief CPU-side texture data (loaded from file)
struct TextureData
{
    std::vector<uint8_t>    pixels;         // Raw pixel data (RGBA)
    uint32_t                width = 0;
    uint32_t                height = 0;
    uint32_t                channels = 4;   // Always 4 after stb conversion
    RHIFormat               format = RHIFormat::RGBA8_UNORM;
    bool                    isSRGB = false; // Hint for format selection
    std::string             sourcePath;     // Original file path

    /// @brief Calculate expected size in bytes
    size_t getSizeBytes() const { return static_cast<size_t>(width) * height * channels; }

    /// @brief Check validity
    bool isValid() const { return !pixels.empty() && width > 0 && height > 0; }
};

/// @brief GPU texture resource wrapper
class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /// @brief Create texture from CPU data (immediate, blocking upload)
    /// @param rhi RHI instance
    /// @param data CPU texture data
    /// @param generateMips Whether to generate mipmaps (future)
    /// @param debugName Debug name for GPU resource
    /// @return Shared pointer to texture, nullptr on failure
    static std::shared_ptr<Texture> create(
        RHI* rhi,
        const TextureData& data,
        bool generateMips = false,
        const char* debugName = nullptr
    );

    /// @brief Create from pre-existing RHI resources (for special textures)
    static std::shared_ptr<Texture> createFromHandles(
        RHI* rhi,
        RHITextureHandle texture,
        RHISamplerHandle sampler,
        uint32_t width,
        uint32_t height,
        RHIFormat format
    );

    // Accessors
    RHITextureHandle getTexture() const { return m_texture; }
    RHISamplerHandle getSampler() const { return m_sampler; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    RHIFormat getFormat() const { return m_format; }
    bool isValid() const { return m_texture && m_sampler; }

private:
    RHI*                m_rhi = nullptr;
    RHITextureHandle    m_texture;
    RHISamplerHandle    m_sampler;
    uint32_t            m_width = 0;
    uint32_t            m_height = 0;
    RHIFormat           m_format = RHIFormat::RGBA8_UNORM;
};

using TexturePtr = std::shared_ptr<Texture>;

} // namespace vesper
