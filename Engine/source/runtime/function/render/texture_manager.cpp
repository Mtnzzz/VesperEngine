#include "texture_manager.h"
#include "runtime/core/log/log_system.h"

#include <stb_image.h>

namespace vesper {

TextureManager::~TextureManager()
{
    shutdown();
}

bool TextureManager::initialize(RHI* rhi, WorkerPool* workerPool)
{
    if (m_initialized)
    {
        LOG_WARN("TextureManager::initialize: Already initialized");
        return true;
    }

    if (!rhi)
    {
        LOG_ERROR("TextureManager::initialize: RHI is null");
        return false;
    }

    m_rhi = rhi;
    m_workerPool = workerPool;

    // Create default textures
    createDefaultTextures();

    m_initialized = true;
    LOG_INFO("TextureManager initialized (async loading: {})", workerPool != nullptr);
    return true;
}

void TextureManager::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    // Clear pending uploads
    {
        std::lock_guard lock(m_uploadMutex);
        while (!m_pendingUploads.empty())
        {
            m_pendingUploads.pop();
        }
    }

    // Clear cache
    clearCache();

    // Release default textures
    m_placeholderTexture.reset();
    m_defaultWhite.reset();
    m_defaultBlack.reset();
    m_defaultNormal.reset();

    m_rhi = nullptr;
    m_workerPool = nullptr;
    m_initialized = false;

    LOG_INFO("TextureManager shutdown");
}

// =============================================================================
// Synchronous Loading
// =============================================================================

TexturePtr TextureManager::loadTextureSync(const std::string& path, bool isSRGB, const char* debugName)
{
    if (!m_initialized)
    {
        LOG_ERROR("TextureManager::loadTextureSync: Not initialized");
        return m_placeholderTexture;
    }

    // Check cache first
    {
        std::lock_guard lock(m_cacheMutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end())
        {
            return it->second;
        }
    }

    // Load from file
    TextureData data = loadTextureDataFromFile(path, isSRGB);
    if (!data.isValid())
    {
        LOG_WARN("TextureManager::loadTextureSync: Failed to load '{}', using placeholder", path);
        return m_placeholderTexture;
    }

    // Create GPU texture
    TexturePtr texture = Texture::create(m_rhi, data, false, debugName ? debugName : path.c_str());
    if (!texture)
    {
        LOG_ERROR("TextureManager::loadTextureSync: Failed to create GPU texture for '{}'", path);
        return m_placeholderTexture;
    }

    // Add to cache
    {
        std::lock_guard lock(m_cacheMutex);
        m_cache[path] = texture;
    }

    return texture;
}

// =============================================================================
// Asynchronous Loading
// =============================================================================

WaitGroupPtr TextureManager::loadTextureAsync(const std::string& path, bool isSRGB,
                                               std::function<void(TexturePtr)> callback)
{
    if (!m_initialized)
    {
        LOG_ERROR("TextureManager::loadTextureAsync: Not initialized");
        if (callback)
        {
            callback(m_placeholderTexture);
        }
        return nullptr;
    }

    // Check cache first
    {
        std::lock_guard lock(m_cacheMutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end())
        {
            if (callback)
            {
                callback(it->second);
            }
            return nullptr; // Already loaded, no wait needed
        }
    }

    // If no worker pool, fall back to sync loading
    if (!m_workerPool)
    {
        TexturePtr tex = loadTextureSync(path, isSRGB, path.c_str());
        if (callback)
        {
            callback(tex);
        }
        return nullptr;
    }

    // Submit async load task
    // Capture path by value to ensure it lives until task completion
    std::string pathCopy = path;

    return m_workerPool->submit(
        [this, pathCopy, isSRGB, cb = std::move(callback)]()
        {
            // Stage 1: Load file on worker thread
            TextureData data = loadTextureDataFromFile(pathCopy, isSRGB);

            if (!data.isValid())
            {
                LOG_WARN("TextureManager: Async load failed for '{}'", pathCopy);
                // Queue a failed request so callback still fires with placeholder
                {
                    std::lock_guard lock(m_uploadMutex);
                    TextureUploadRequest request;
                    request.cachePath = pathCopy;
                    request.callback = cb;
                    request.isSRGB = isSRGB;
                    // Empty data will trigger placeholder usage
                    m_pendingUploads.push(std::move(request));
                }
                return;
            }

            // Queue for GPU upload on render thread
            {
                std::lock_guard lock(m_uploadMutex);
                TextureUploadRequest request;
                request.data = std::move(data);
                request.cachePath = pathCopy;
                request.callback = cb;
                request.isSRGB = isSRGB;
                m_pendingUploads.push(std::move(request));
            }
        },
        TaskAffinity::AnyThread,
        TaskPriority::Normal
    );
}

uint32_t TextureManager::processPendingUploads(uint32_t maxUploads)
{
    if (!m_initialized)
    {
        return 0;
    }

    uint32_t uploadCount = 0;

    while (uploadCount < maxUploads)
    {
        TextureUploadRequest request;

        // Get next pending upload
        {
            std::lock_guard lock(m_uploadMutex);
            if (m_pendingUploads.empty())
            {
                break;
            }
            request = std::move(m_pendingUploads.front());
            m_pendingUploads.pop();
        }

        TexturePtr texture;

        // Check if already in cache (could have been loaded by another request)
        {
            std::lock_guard lock(m_cacheMutex);
            auto it = m_cache.find(request.cachePath);
            if (it != m_cache.end())
            {
                texture = it->second;
            }
        }

        if (!texture)
        {
            if (request.data.isValid())
            {
                // Create GPU texture
                texture = Texture::create(m_rhi, request.data, false, request.cachePath.c_str());
                if (texture)
                {
                    std::lock_guard lock(m_cacheMutex);
                    m_cache[request.cachePath] = texture;
                }
            }

            // Use placeholder if creation failed
            if (!texture)
            {
                texture = m_placeholderTexture;
            }
        }

        // Fire callback
        if (request.callback)
        {
            request.callback(texture);
        }

        ++uploadCount;
    }

    return uploadCount;
}

// =============================================================================
// Cache Management
// =============================================================================

TexturePtr TextureManager::getCached(const std::string& path) const
{
    std::lock_guard lock(m_cacheMutex);
    auto it = m_cache.find(path);
    return (it != m_cache.end()) ? it->second : nullptr;
}

bool TextureManager::isCached(const std::string& path) const
{
    std::lock_guard lock(m_cacheMutex);
    return m_cache.find(path) != m_cache.end();
}

void TextureManager::uncache(const std::string& path)
{
    std::lock_guard lock(m_cacheMutex);
    m_cache.erase(path);
}

void TextureManager::clearCache()
{
    std::lock_guard lock(m_cacheMutex);
    m_cache.clear();
}

size_t TextureManager::cacheSize() const
{
    std::lock_guard lock(m_cacheMutex);
    return m_cache.size();
}

// =============================================================================
// Utility
// =============================================================================

TextureData TextureManager::loadTextureDataFromFile(const std::string& path, bool isSRGB)
{
    TextureData data;
    data.sourcePath = path;
    data.isSRGB = isSRGB;

    int width, height, channels;

    // Always request RGBA (4 channels)
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
    {
        LOG_ERROR("TextureManager: stbi_load failed for '{}': {}", path, stbi_failure_reason());
        return data; // Empty/invalid
    }

    data.width = static_cast<uint32_t>(width);
    data.height = static_cast<uint32_t>(height);
    data.channels = 4; // Always RGBA after stb conversion
    data.format = isSRGB ? RHIFormat::RGBA8_SRGB : RHIFormat::RGBA8_UNORM;

    // Copy pixel data
    size_t sizeBytes = data.getSizeBytes();
    data.pixels.resize(sizeBytes);
    std::memcpy(data.pixels.data(), pixels, sizeBytes);

    // Free stb-allocated memory
    stbi_image_free(pixels);

    LOG_DEBUG("TextureManager: Loaded '{}' ({}x{}, {} channels)", path, width, height, channels);
    return data;
}

// =============================================================================
// Default Textures
// =============================================================================

void TextureManager::createDefaultTextures()
{
    // White texture (1,1,1,1)
    m_defaultWhite = createSolidColorTexture(255, 255, 255, 255, false, "DefaultWhite");

    // Black texture (0,0,0,1)
    m_defaultBlack = createSolidColorTexture(0, 0, 0, 255, false, "DefaultBlack");

    // Default normal map (flat normal: 128,128,255 = 0,0,1 in tangent space)
    m_defaultNormal = createSolidColorTexture(128, 128, 255, 255, false, "DefaultNormal");

    // Placeholder (magenta/black checkerboard)
    m_placeholderTexture = createPlaceholderTexture();

    LOG_INFO("TextureManager: Created default textures");
}

TexturePtr TextureManager::createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                                    bool isSRGB, const char* debugName)
{
    TextureData data;
    data.width = 1;
    data.height = 1;
    data.channels = 4;
    data.isSRGB = isSRGB;
    data.pixels = {r, g, b, a};

    return Texture::create(m_rhi, data, false, debugName);
}

TexturePtr TextureManager::createPlaceholderTexture()
{
    // 8x8 checkerboard pattern (magenta and black)
    constexpr uint32_t size = 8;
    TextureData data;
    data.width = size;
    data.height = size;
    data.channels = 4;
    data.isSRGB = true;
    data.pixels.resize(size * size * 4);

    for (uint32_t y = 0; y < size; ++y)
    {
        for (uint32_t x = 0; x < size; ++x)
        {
            size_t idx = (y * size + x) * 4;
            bool isLight = ((x + y) % 2) == 0;

            if (isLight)
            {
                // Magenta
                data.pixels[idx + 0] = 255;
                data.pixels[idx + 1] = 0;
                data.pixels[idx + 2] = 255;
                data.pixels[idx + 3] = 255;
            }
            else
            {
                // Black
                data.pixels[idx + 0] = 0;
                data.pixels[idx + 1] = 0;
                data.pixels[idx + 2] = 0;
                data.pixels[idx + 3] = 255;
            }
        }
    }

    return Texture::create(m_rhi, data, false, "PlaceholderTexture");
}

} // namespace vesper
