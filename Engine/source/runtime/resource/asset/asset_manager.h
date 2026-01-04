#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace vesper {

// Asset handle type
using AssetHandle = uint32_t;
constexpr AssetHandle InvalidAssetHandle = 0;

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    void initialize();
    void shutdown();

    // Asset loading via JSON + reflection serializers
    template<typename T>
    std::shared_ptr<T> loadAsset(const std::string& path);

    template<typename T>
    bool saveAsset(const std::string& path, const T& asset);

    // Asset caching
    bool isAssetLoaded(const std::string& path) const;
    void unloadAsset(const std::string& path);
    void unloadAllAssets();

private:
    std::unordered_map<std::string, std::shared_ptr<void>> m_asset_cache;
};

} // namespace vesper
