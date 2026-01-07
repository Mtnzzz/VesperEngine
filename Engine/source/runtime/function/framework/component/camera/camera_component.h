#pragma once

#include "runtime/function/render/camera.h"

#include <memory>

namespace vesper {

/// @brief Camera component - wraps Camera for ECS usage
struct CameraComponent
{
    // =========================================================================
    // Camera Reference
    // =========================================================================

    /// @brief Shared camera instance
    std::shared_ptr<Camera> camera;

    // =========================================================================
    // State Flags
    // =========================================================================

    /// @brief Whether this is the active/main camera
    bool active{true};

    /// @brief Priority for camera selection (higher = more likely to be main)
    int32_t priority{0};

    // =========================================================================
    // Render Target (optional)
    // =========================================================================

    /// @brief Render target ID (0 = main viewport)
    uint32_t renderTargetId{0};

    // =========================================================================
    // Helpers
    // =========================================================================

    /// @brief Check if camera is valid
    bool isValid() const
    {
        return camera != nullptr;
    }

    /// @brief Create with new camera instance
    static CameraComponent create(const CameraConfig& config = CameraConfig())
    {
        CameraComponent comp;
        comp.camera = std::make_shared<Camera>(config);
        return comp;
    }

    /// @brief Create with existing camera
    static CameraComponent fromCamera(std::shared_ptr<Camera> existingCamera)
    {
        CameraComponent comp;
        comp.camera = std::move(existingCamera);
        return comp;
    }
};

} // namespace vesper
