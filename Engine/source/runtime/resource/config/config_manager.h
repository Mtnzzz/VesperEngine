#pragma once

#include <string>

namespace vesper {

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    void initialize(const std::string& config_file_path);
    void shutdown();

    // Path accessors
    const std::string& getRootFolder() const;
    const std::string& getAssetFolder() const;
    const std::string& getSchemaFolder() const;
    const std::string& getEditorAssetFolder() const;

    // Config URLs
    const std::string& getDefaultWorldUrl() const;
    const std::string& getGlobalRenderingUrl() const;
    const std::string& getGlobalParticleUrl() const;

private:
    std::string m_root_folder;
    std::string m_asset_folder;
    std::string m_schema_folder;
    std::string m_editor_asset_folder;
    std::string m_default_world_url;
    std::string m_global_rendering_url;
    std::string m_global_particle_url;
};

} // namespace vesper
