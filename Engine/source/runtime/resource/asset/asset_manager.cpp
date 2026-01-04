#include "asset_manager.h"

namespace vesper {

AssetManager::AssetManager() = default;
AssetManager::~AssetManager() = default;

void AssetManager::initialize() {
    // TODO: Initialize asset loading system
}

void AssetManager::shutdown() {
    unloadAllAssets();
}

bool AssetManager::isAssetLoaded(const std::string& path) const {
    return m_asset_cache.find(path) != m_asset_cache.end();
}

void AssetManager::unloadAsset(const std::string& path) {
    m_asset_cache.erase(path);
}

void AssetManager::unloadAllAssets() {
    m_asset_cache.clear();
}

} // namespace vesper
