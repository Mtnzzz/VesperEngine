#include "config_manager.h"

namespace vesper {

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

void ConfigManager::initialize(const std::string& config_file_path) {
    // TODO: Load config from INI file
}

void ConfigManager::shutdown() {
    // TODO: Cleanup
}

const std::string& ConfigManager::getRootFolder() const {
    return m_root_folder;
}

const std::string& ConfigManager::getAssetFolder() const {
    return m_asset_folder;
}

const std::string& ConfigManager::getSchemaFolder() const {
    return m_schema_folder;
}

const std::string& ConfigManager::getEditorAssetFolder() const {
    return m_editor_asset_folder;
}

const std::string& ConfigManager::getDefaultWorldUrl() const {
    return m_default_world_url;
}

const std::string& ConfigManager::getGlobalRenderingUrl() const {
    return m_global_rendering_url;
}

const std::string& ConfigManager::getGlobalParticleUrl() const {
    return m_global_particle_url;
}

} // namespace vesper
