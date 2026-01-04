#pragma once

#include "runtime/function/render/texture.h"
#include "runtime/core/threading/worker_pool.h"

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

namespace vesper {

/// @brief Pending texture upload data (CPU data waiting for GPU upload)
struct TextureUploadRequest
{
    TextureData data;                              // CPU pixel data
    std::string cachePath;                         // Cache key
    std::function<void(TexturePtr)> callback;      // Completion callback
    bool isSRGB = true;                            // Format hint
};

/// @brief Global texture manager with caching and async loading support
///
/// Usage:
/// - Synchronous: auto tex = texMgr->loadTextureSync("path/to/texture.png");
/// - Asynchronous: texMgr->loadTextureAsync("path/to/texture.png", true, [](TexturePtr tex) { ... });
///
/// The async loading flow:
/// 1. Worker thread: stbi_load() reads file into CPU memory
/// 2. Worker thread: Creates TextureUploadRequest and queues it
/// 3. Render thread: processPendingUploads() creates GPU resources
/// 4. Render thread: Triggers callback with completed texture
class TextureManager
{
public:
    TextureManager() = default;
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    /// @brief Initialize the texture manager
    /// @param rhi RHI instance for GPU resource creation
    /// @param workerPool Worker pool for async loading (optional, sync-only if null)
    /// @return true if initialization succeeded
    bool initialize(RHI* rhi, WorkerPool* workerPool = nullptr);

    /// @brief Shutdown and release all resources
    void shutdown();

    // =========================================================================
    // Synchronous Loading
    // =========================================================================

    /// @brief Load texture synchronously (blocks until complete)
    /// @param path File path to texture
    /// @param isSRGB Whether to treat as sRGB (true for color textures)
    /// @param debugName Optional debug name for GPU resource
    /// @return Texture pointer, or placeholder on failure
    TexturePtr loadTextureSync(const std::string& path, bool isSRGB = true,
                               const char* debugName = nullptr);

    // =========================================================================
    // Asynchronous Loading
    // =========================================================================

    /// @brief Load texture asynchronously
    /// @param path File path to texture
    /// @param isSRGB Whether to treat as sRGB
    /// @param callback Called on render thread when texture is ready
    /// @return WaitGroup that signals when load is complete (may be null if already cached)
    WaitGroupPtr loadTextureAsync(const std::string& path, bool isSRGB,
                                  std::function<void(TexturePtr)> callback);

    /// @brief Process pending GPU uploads (call from render thread each frame)
    /// @param maxUploads Maximum number of uploads to process per call
    /// @return Number of textures uploaded
    uint32_t processPendingUploads(uint32_t maxUploads = 4);

    // =========================================================================
    // Cache Management
    // =========================================================================

    /// @brief Get cached texture by path
    /// @return Texture if cached, nullptr otherwise
    TexturePtr getCached(const std::string& path) const;

    /// @brief Check if texture is in cache
    bool isCached(const std::string& path) const;

    /// @brief Remove texture from cache (texture may still exist if referenced elsewhere)
    void uncache(const std::string& path);

    /// @brief Clear entire cache
    void clearCache();

    /// @brief Get number of cached textures
    size_t cacheSize() const;

    // =========================================================================
    // Default Textures
    // =========================================================================

    /// @brief Get placeholder texture (magenta/black checkerboard for missing textures)
    TexturePtr getPlaceholder() const { return m_placeholderTexture; }

    /// @brief Get default white texture (1,1,1,1)
    TexturePtr getDefaultWhite() const { return m_defaultWhite; }

    /// @brief Get default black texture (0,0,0,1)
    TexturePtr getDefaultBlack() const { return m_defaultBlack; }

    /// @brief Get default normal map texture (128,128,255 = flat normal)
    TexturePtr getDefaultNormal() const { return m_defaultNormal; }

    // =========================================================================
    // Utility
    // =========================================================================

    /// @brief Load raw texture data from file (CPU only, no GPU upload)
    /// @param path File path
    /// @param isSRGB Format hint
    /// @return TextureData with pixel data, or empty on failure
    static TextureData loadTextureDataFromFile(const std::string& path, bool isSRGB = true);

    /// @brief Check if manager is initialized
    bool isInitialized() const { return m_initialized; }

private:
    /// @brief Create default placeholder textures
    void createDefaultTextures();

    /// @brief Create a solid color 1x1 texture
    TexturePtr createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                       bool isSRGB, const char* debugName);

    /// @brief Create checkerboard placeholder texture
    TexturePtr createPlaceholderTexture();

private:
    RHI* m_rhi = nullptr;
    WorkerPool* m_workerPool = nullptr;
    bool m_initialized = false;

    // Texture cache (path -> texture)
    mutable std::mutex m_cacheMutex;
    std::unordered_map<std::string, TexturePtr> m_cache;

    // Pending uploads queue (written by worker threads, consumed by render thread)
    mutable std::mutex m_uploadMutex;
    std::queue<TextureUploadRequest> m_pendingUploads;

    // Default textures
    TexturePtr m_placeholderTexture;
    TexturePtr m_defaultWhite;
    TexturePtr m_defaultBlack;
    TexturePtr m_defaultNormal;
};

} // namespace vesper
