#pragma once

#include "runtime/function/render/model.h"
#include "runtime/function/render/texture_manager.h"
#include "runtime/core/threading/worker_pool.h"

#include <functional>
#include <memory>
#include <string>

namespace vesper {

/// @brief Model loading options
struct ModelLoadOptions
{
    bool loadMaterials = true;         // Load and create materials
    bool loadTextures = true;          // Load textures (requires loadMaterials)
    bool calculateTangents = true;     // Calculate tangent vectors for normal mapping
    bool flipUVs = true;               // Flip UV coordinates (common for OpenGL -> Vulkan)
    bool generateNormals = false;      // Generate normals if missing
    bool optimizeMeshes = true;        // Optimize mesh data
    bool joinIdenticalVertices = true; // Merge identical vertices
    bool triangulate = true;           // Convert polygons to triangles
    float scaleFactor = 1.0f;          // Global scale factor
};

/// @brief Model loading result
struct ModelLoadResult
{
    ModelPtr model;
    bool success = false;
    std::string errorMessage;

    explicit operator bool() const { return success; }
};

/// @brief Assimp-based model loader
///
/// Usage:
/// ```cpp
/// ModelLoader loader;
/// loader.initialize(rhi, textureManager, workerPool);
///
/// // Synchronous loading
/// auto result = loader.loadSync("models/cube.obj");
/// if (result) {
///     // Use result.model
/// }
///
/// // Asynchronous loading
/// loader.loadAsync("models/character.fbx", {}, [](ModelLoadResult result) {
///     if (result) {
///         // Use result.model
///     }
/// });
/// ```
class ModelLoader
{
public:
    ModelLoader() = default;
    ~ModelLoader() = default;

    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;

    /// @brief Initialize the model loader
    /// @param rhi RHI instance for GPU resource creation
    /// @param textureManager Texture manager for loading textures (optional)
    /// @param workerPool Worker pool for async loading (optional)
    bool initialize(RHI* rhi, TextureManager* textureManager = nullptr,
                    WorkerPool* workerPool = nullptr);

    /// @brief Shutdown and release resources
    void shutdown();

    // =========================================================================
    // Synchronous Loading
    // =========================================================================

    /// @brief Load model synchronously
    /// @param path File path to model
    /// @param options Loading options
    /// @return Loading result with model or error message
    ModelLoadResult loadSync(const std::string& path,
                             const ModelLoadOptions& options = {});

    // =========================================================================
    // Asynchronous Loading
    // =========================================================================

    /// @brief Load model asynchronously
    /// @param path File path to model
    /// @param options Loading options
    /// @param callback Called when loading is complete (on render thread)
    /// @return WaitGroup for synchronization (may be null if no worker pool)
    WaitGroupPtr loadAsync(const std::string& path,
                           const ModelLoadOptions& options,
                           std::function<void(ModelLoadResult)> callback);

    // =========================================================================
    // Utility
    // =========================================================================

    /// @brief Check if file format is supported
    static bool isFormatSupported(const std::string& extension);

    /// @brief Get list of supported file extensions
    static std::vector<std::string> getSupportedExtensions();

    /// @brief Check if loader is initialized
    bool isInitialized() const { return m_initialized; }

private:
    /// @brief Internal loading implementation
    ModelLoadResult loadInternal(const std::string& path,
                                 const ModelLoadOptions& options);

    /// @brief Get directory from file path
    static std::string getDirectory(const std::string& path);

    /// @brief Resolve texture path, searching multiple directories
    static std::string resolveTexturePath(const std::string& modelDir, const std::string& texturePath);

private:
    RHI* m_rhi = nullptr;
    TextureManager* m_textureManager = nullptr;
    WorkerPool* m_workerPool = nullptr;
    bool m_initialized = false;
};

} // namespace vesper
