#pragma once

#include <vector>
#include <cstdint>

namespace vesper {

// Backend-agnostic render object descriptor
struct RenderObjectDesc {
    uint32_t    object_id       = 0;
    uint32_t    mesh_id         = 0;
    uint32_t    material_id     = 0;
    // Transform matrix (column-major)
    float       transform[16]   = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    // Bounding sphere (center + radius)
    float       bounding_sphere[4] = {0, 0, 0, 1};
};

// Camera parameters for rendering
struct CameraParams {
    float view_matrix[16];
    float proj_matrix[16];
    float position[3];
    float fov;
    float near_plane;
    float far_plane;
};

// Skinning matrices for skeletal animation
struct SkinningMatrices {
    uint32_t            object_id;
    std::vector<float>  joint_matrices;  // 4x4 matrices, column-major
};

// Particle emission request
struct ParticleRequest {
    uint32_t    emitter_id;
    float       position[3];
    float       direction[3];
    float       spawn_rate;
};

// Double-buffered swap context for logic/render thread communication
struct RenderSwapContext {
    // Level resource loading
    bool                            level_resource_changed = false;
    std::vector<uint32_t>           mesh_ids_to_load;
    std::vector<uint32_t>           texture_ids_to_load;

    // Game object updates
    std::vector<RenderObjectDesc>   objects_to_add;
    std::vector<RenderObjectDesc>   objects_to_update;
    std::vector<uint32_t>           objects_to_delete;

    // Camera
    CameraParams                    camera_params;

    // Animation
    std::vector<SkinningMatrices>   skinning_updates;

    // Particles
    std::vector<ParticleRequest>    particle_requests;

    // Frame info
    uint64_t                        frame_index = 0;

    void clear() {
        level_resource_changed = false;
        mesh_ids_to_load.clear();
        texture_ids_to_load.clear();
        objects_to_add.clear();
        objects_to_update.clear();
        objects_to_delete.clear();
        skinning_updates.clear();
        particle_requests.clear();
    }
};

} // namespace vesper