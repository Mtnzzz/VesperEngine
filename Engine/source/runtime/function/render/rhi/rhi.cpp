#include "rhi.h"
#include "runtime/function/render/backend/vulkan/vulkan_rhi.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

std::unique_ptr<RHI> createRHI(RHIBackendType type)
{
    switch (type) {
        case RHIBackendType::Vulkan:
            LOG_INFO("Creating Vulkan RHI backend");
            return std::make_unique<VulkanRHI>();

        case RHIBackendType::D3D12:
            LOG_ERROR("D3D12 backend not yet implemented");
            return nullptr;

        case RHIBackendType::Metal:
            LOG_ERROR("Metal backend not yet implemented");
            return nullptr;

        default:
            LOG_ERROR("Unknown RHI backend type");
            return nullptr;
    }
}

} // namespace vesper
