#include "model_loader.h"
#include "runtime/core/log/log_system.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <algorithm>

namespace vesper {

bool ModelLoader::initialize(RHI* rhi, TextureManager* textureManager, WorkerPool* workerPool)
{
    if (m_initialized)
    {
        LOG_WARN("ModelLoader::initialize: Already initialized");
        return true;
    }

    if (!rhi)
    {
        LOG_ERROR("ModelLoader::initialize: RHI is null");
        return false;
    }

    m_rhi = rhi;
    m_textureManager = textureManager;
    m_workerPool = workerPool;
    m_initialized = true;

    LOG_INFO("ModelLoader initialized (async: {}, textures: {})",
             workerPool != nullptr, textureManager != nullptr);
    return true;
}

void ModelLoader::shutdown()
{
    m_rhi = nullptr;
    m_textureManager = nullptr;
    m_workerPool = nullptr;
    m_initialized = false;
    LOG_INFO("ModelLoader shutdown");
}

// =============================================================================
// Synchronous Loading
// =============================================================================

ModelLoadResult ModelLoader::loadSync(const std::string& path, const ModelLoadOptions& options)
{
    if (!m_initialized)
    {
        return ModelLoadResult{nullptr, false, "ModelLoader not initialized"};
    }

    return loadInternal(path, options);
}

// =============================================================================
// Asynchronous Loading
// =============================================================================

WaitGroupPtr ModelLoader::loadAsync(const std::string& path,
                                     const ModelLoadOptions& options,
                                     std::function<void(ModelLoadResult)> callback)
{
    if (!m_initialized)
    {
        if (callback)
        {
            callback(ModelLoadResult{nullptr, false, "ModelLoader not initialized"});
        }
        return nullptr;
    }

    // Fallback to sync if no worker pool
    if (!m_workerPool)
    {
        auto result = loadInternal(path, options);
        if (callback)
        {
            callback(result);
        }
        return nullptr;
    }

    // Submit async loading task
    std::string pathCopy = path;
    ModelLoadOptions optionsCopy = options;

    return m_workerPool->submit(
        [this, pathCopy, optionsCopy, cb = std::move(callback)]()
        {
            auto result = loadInternal(pathCopy, optionsCopy);
            if (cb)
            {
                cb(result);
            }
        },
        TaskAffinity::AnyThread,
        TaskPriority::Normal
    );
}

// =============================================================================
// Internal Loading
// =============================================================================

ModelLoadResult ModelLoader::loadInternal(const std::string& path, const ModelLoadOptions& options)
{
    ModelLoadResult result;

    // Check if file exists
    if (!std::filesystem::exists(path))
    {
        result.errorMessage = "File not found: " + path;
        LOG_ERROR("ModelLoader: {}", result.errorMessage);
        return result;
    }

    // Build Assimp post-processing flags
    unsigned int flags = 0;

    if (options.triangulate)
        flags |= aiProcess_Triangulate;

    if (options.flipUVs)
        flags |= aiProcess_FlipUVs;

    if (options.calculateTangents)
        flags |= aiProcess_CalcTangentSpace;

    if (options.generateNormals)
        flags |= aiProcess_GenSmoothNormals;

    if (options.joinIdenticalVertices)
        flags |= aiProcess_JoinIdenticalVertices;

    if (options.optimizeMeshes)
        flags |= aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;

    // Always useful
    flags |= aiProcess_ImproveCacheLocality;
    flags |= aiProcess_SortByPType;  // Split meshes by primitive type

    // Create Assimp importer
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        result.errorMessage = "Assimp error: " + std::string(importer.GetErrorString());
        LOG_ERROR("ModelLoader: {}", result.errorMessage);
        return result;
    }

    // Create model
    result.model = std::make_shared<Model>();
    result.model->setSourcePath(path);

    // Extract filename as model name
    std::filesystem::path filePath(path);
    result.model->setName(filePath.stem().string());

    std::string directory = getDirectory(path);

    // Process materials first (if enabled)
    std::vector<MaterialPtr> materials;

    if (options.loadMaterials && scene->HasMaterials())
    {
        materials.reserve(scene->mNumMaterials);

        for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
        {
            const aiMaterial* aiMat = scene->mMaterials[i];

            MaterialData matData;

            // Get material name
            aiString matName;
            if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
            {
                matData.albedoPath = ""; // Will set texture paths below
            }

            // Base color
            aiColor4D color;
            if (aiMat->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
                aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            {
                matData.baseColor = glm::vec4(color.r, color.g, color.b, color.a);
            }

            // Metallic/Roughness
            float metallic = 0.0f, roughness = 1.0f;
            aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            matData.metallicFactor = metallic;
            matData.roughnessFactor = roughness;

            // Load textures if enabled
            if (options.loadTextures && m_textureManager)
            {
                aiString texPath;

                // Albedo/Diffuse texture
                if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
                    aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
                {
                    matData.albedoPath = resolveTexturePath(directory, texPath.C_Str());
                }

                // Normal map
                if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
                    aiMat->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS)
                {
                    matData.normalPath = resolveTexturePath(directory, texPath.C_Str());
                }

                // Metallic texture (often combined with roughness in glTF)
                if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS)
                {
                    matData.metallicPath = resolveTexturePath(directory, texPath.C_Str());
                }

                // Roughness texture
                if (aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS ||
                    aiMat->GetTexture(aiTextureType_SHININESS, 0, &texPath) == AI_SUCCESS)
                {
                    matData.roughnessPath = resolveTexturePath(directory, texPath.C_Str());
                }

                // AO texture
                if (aiMat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS ||
                    aiMat->GetTexture(aiTextureType_LIGHTMAP, 0, &texPath) == AI_SUCCESS)
                {
                    matData.aoPath = resolveTexturePath(directory, texPath.C_Str());
                }
            }

            // Create material
            std::string debugName = matName.length > 0 ? matName.C_Str() : ("Material_" + std::to_string(i));
            MaterialPtr material = Material::create(m_rhi, matData, m_textureManager, debugName.c_str());
            materials.push_back(material);
        }
    }

    // Process meshes
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* aiMesh = scene->mMeshes[i];

        // Skip non-triangle meshes
        if (!(aiMesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE))
        {
            continue;
        }

        SubMesh submesh;
        submesh.name = aiMesh->mName.C_Str();

        // Build vertex data
        std::vector<ModelVertex> vertices;
        vertices.reserve(aiMesh->mNumVertices);

        glm::vec3 boundsMin(std::numeric_limits<float>::max());
        glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

        for (unsigned int v = 0; v < aiMesh->mNumVertices; ++v)
        {
            ModelVertex vertex{};

            // Position
            vertex.position = glm::vec3(
                aiMesh->mVertices[v].x * options.scaleFactor,
                aiMesh->mVertices[v].y * options.scaleFactor,
                aiMesh->mVertices[v].z * options.scaleFactor
            );

            // Update bounds
            boundsMin = glm::min(boundsMin, vertex.position);
            boundsMax = glm::max(boundsMax, vertex.position);

            // Normal
            if (aiMesh->HasNormals())
            {
                vertex.normal = glm::vec3(
                    aiMesh->mNormals[v].x,
                    aiMesh->mNormals[v].y,
                    aiMesh->mNormals[v].z
                );
            }

            // UV coordinates (first set only)
            if (aiMesh->HasTextureCoords(0))
            {
                vertex.texCoord = glm::vec2(
                    aiMesh->mTextureCoords[0][v].x,
                    aiMesh->mTextureCoords[0][v].y
                );
            }

            // Tangent
            if (aiMesh->HasTangentsAndBitangents())
            {
                glm::vec3 tangent(
                    aiMesh->mTangents[v].x,
                    aiMesh->mTangents[v].y,
                    aiMesh->mTangents[v].z
                );
                glm::vec3 bitangent(
                    aiMesh->mBitangents[v].x,
                    aiMesh->mBitangents[v].y,
                    aiMesh->mBitangents[v].z
                );

                // Calculate handedness
                float handedness = glm::dot(
                    glm::cross(vertex.normal, tangent),
                    bitangent
                ) < 0.0f ? -1.0f : 1.0f;

                vertex.tangent = glm::vec4(tangent, handedness);
            }

            vertices.push_back(vertex);
        }

        submesh.boundsMin = boundsMin;
        submesh.boundsMax = boundsMax;

        // Build index data
        std::vector<uint32_t> indices;
        indices.reserve(aiMesh->mNumFaces * 3);

        for (unsigned int f = 0; f < aiMesh->mNumFaces; ++f)
        {
            const aiFace& face = aiMesh->mFaces[f];
            for (unsigned int idx = 0; idx < face.mNumIndices; ++idx)
            {
                indices.push_back(face.mIndices[idx]);
            }
        }

        // Create MeshData
        MeshData meshData;
        meshData.setVerticesWithLayout(vertices, ModelVertex::getVertexInputState());
        meshData.indices = std::move(indices);

        // Create GPU mesh
        std::string meshDebugName = submesh.name.empty() ? ("Mesh_" + std::to_string(i)) : submesh.name;
        submesh.mesh = Mesh::create(m_rhi, meshData, meshDebugName.c_str());

        if (!submesh.mesh)
        {
            LOG_WARN("ModelLoader: Failed to create mesh '{}'", meshDebugName);
            continue;
        }

        // Assign material
        if (aiMesh->mMaterialIndex < materials.size())
        {
            submesh.material = materials[aiMesh->mMaterialIndex];
        }

        result.model->addSubMesh(std::move(submesh));
    }

    // Recalculate overall bounds
    result.model->recalculateBounds();

    // Process node hierarchy (simplified - just store structure)
    std::function<void(const aiNode*, int32_t)> processNode;
    processNode = [&](const aiNode* node, int32_t parentIndex)
    {
        ModelNode modelNode;
        modelNode.name = node->mName.C_Str();
        modelNode.parentIndex = parentIndex;

        // Convert Assimp matrix to glm
        const auto& m = node->mTransformation;
        modelNode.localTransform = glm::mat4(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4
        );

        // Store mesh indices
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            modelNode.meshIndices.push_back(node->mMeshes[i]);
        }

        int32_t currentIndex = static_cast<int32_t>(result.model->getNodeCount());
        result.model->addNode(modelNode);

        // Process children
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
        {
            uint32_t childIndex = static_cast<uint32_t>(result.model->getNodeCount());
            result.model->getNode(currentIndex).childIndices.push_back(childIndex);
            processNode(node->mChildren[i], currentIndex);
        }
    };

    if (scene->mRootNode)
    {
        std::vector<uint32_t> rootIndices;
        rootIndices.push_back(0);
        processNode(scene->mRootNode, -1);
        result.model->setRootNodes(rootIndices);
    }

    result.success = true;

    LOG_INFO("ModelLoader: Loaded '{}' ({} submeshes, {} vertices, {} triangles)",
             result.model->getName(),
             result.model->getSubMeshCount(),
             result.model->getTotalVertexCount(),
             result.model->getTotalIndexCount() / 3);

    return result;
}

// =============================================================================
// Utility
// =============================================================================

bool ModelLoader::isFormatSupported(const std::string& extension)
{
    static const std::vector<std::string> supported = {
        ".obj", ".gltf", ".glb", ".fbx", ".blend", ".dae", ".3ds", ".ply", ".stl"
    };

    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (!ext.empty() && ext[0] != '.')
    {
        ext = "." + ext;
    }

    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

std::vector<std::string> ModelLoader::getSupportedExtensions()
{
    return {".obj", ".gltf", ".glb", ".fbx", ".blend", ".dae", ".3ds", ".ply", ".stl"};
}

std::string ModelLoader::getDirectory(const std::string& path)
{
    std::filesystem::path fsPath(path);
    return fsPath.parent_path().string();
}

std::string ModelLoader::resolveTexturePath(const std::string& modelDir, const std::string& texturePath)
{
    // Handle embedded textures (starting with *)
    if (!texturePath.empty() && texturePath[0] == '*')
    {
        return texturePath;  // Embedded texture, return as-is
    }

    std::filesystem::path texPath(texturePath);
    std::string filename = texPath.filename().string();
    std::string stem = texPath.stem().string();
    std::string ext = texPath.extension().string();

    // Extension variants to try (jpg <-> jpeg, etc.)
    std::vector<std::string> extVariants = {ext};
    if (ext == ".jpg" || ext == ".JPG")
    {
        extVariants.push_back(".jpeg");
        extVariants.push_back(".JPEG");
    }
    else if (ext == ".jpeg" || ext == ".JPEG")
    {
        extVariants.push_back(".jpg");
        extVariants.push_back(".JPG");
    }

    // Directories to search
    std::vector<std::filesystem::path> searchDirs;

    // 1. Model directory
    searchDirs.push_back(std::filesystem::path(modelDir));

    // 2. Textures folder at same level as models/
    std::filesystem::path modelDirPath(modelDir);
    if (modelDirPath.filename() == "models")
    {
        searchDirs.push_back(modelDirPath.parent_path() / "textures");
    }

    // 3. Replace "models" with "textures" in path
    std::string modelDirStr = modelDir;
    size_t modelsPos = modelDirStr.rfind("models");
    if (modelsPos != std::string::npos)
    {
        std::string texturesDirStr = modelDirStr;
        texturesDirStr.replace(modelsPos, 6, "textures");
        searchDirs.push_back(std::filesystem::path(texturesDirStr));
    }

    // Search in all directories with all filename/extension variants
    for (const auto& dir : searchDirs)
    {
        // Try original path relative to directory
        if (std::filesystem::exists(dir / texturePath))
        {
            return (dir / texturePath).string();
        }

        // Try just filename
        if (std::filesystem::exists(dir / filename))
        {
            return (dir / filename).string();
        }

        // Try stem + extension variants
        for (const auto& extVar : extVariants)
        {
            std::string filenameVar = stem + extVar;
            if (std::filesystem::exists(dir / filenameVar))
            {
                return (dir / filenameVar).string();
            }
        }
    }

    // If not found, return original path (will fail with helpful error message)
    return (std::filesystem::path(modelDir) / texturePath).string();
}

} // namespace vesper
