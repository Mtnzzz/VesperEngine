#include "texture.h"
#include "runtime/core/log/log_system.h"

namespace vesper {

Texture::~Texture()
{
    if (m_rhi)
    {
        if (m_sampler)
        {
            m_rhi->destroySampler(m_sampler);
        }
        if (m_texture)
        {
            m_rhi->destroyTexture(m_texture);
        }
    }
}

std::shared_ptr<Texture> Texture::create(
    RHI* rhi,
    const TextureData& data,
    bool generateMips,
    const char* debugName)
{
    if (!rhi || !data.isValid())
    {
        LOG_ERROR("Texture::create: Invalid RHI or texture data");
        return nullptr;
    }

    auto texture = std::make_shared<Texture>();
    texture->m_rhi = rhi;
    texture->m_width = data.width;
    texture->m_height = data.height;
    texture->m_format = data.isSRGB ? RHIFormat::RGBA8_SRGB : RHIFormat::RGBA8_UNORM;

    // 1. Create staging buffer
    RHIBufferDesc stagingDesc{};
    stagingDesc.size = data.getSizeBytes();
    stagingDesc.usage = RHIBufferUsage::TransferSrc;
    stagingDesc.memoryUsage = RHIMemoryUsage::CpuOnly;
    stagingDesc.debugName = "TextureStagingBuffer";

    auto stagingBuffer = rhi->createBuffer(stagingDesc);
    if (!stagingBuffer)
    {
        LOG_ERROR("Texture::create: Failed to create staging buffer");
        return nullptr;
    }

    // Upload to staging buffer
    rhi->updateBuffer(stagingBuffer, data.pixels.data(), data.getSizeBytes(), 0);

    // 2. Create GPU texture
    RHITextureDesc texDesc{};
    texDesc.extent = {data.width, data.height, 1};
    texDesc.format = texture->m_format;
    texDesc.usage = RHITextureUsage::Sampled | RHITextureUsage::TransferDst;
    texDesc.memoryUsage = RHIMemoryUsage::GpuOnly;
    texDesc.mipLevels = 1; // TODO: generateMips support
    texDesc.debugName = debugName;

    texture->m_texture = rhi->createTexture(texDesc);
    if (!texture->m_texture)
    {
        rhi->destroyBuffer(stagingBuffer);
        LOG_ERROR("Texture::create: Failed to create GPU texture");
        return nullptr;
    }

    // 3. Create one-shot command buffer for upload
    RHICommandPoolDesc poolDesc{};
    poolDesc.queueType = RHIQueueType::Graphics;
    poolDesc.transient = true;
    auto cmdPool = rhi->createCommandPool(poolDesc);
    auto cmd = rhi->allocateCommandBuffer(cmdPool);

    rhi->beginCommandBuffer(cmd);

    // Transition to TransferDst
    RHITextureBarrier barrier{};
    barrier.texture = texture->m_texture;
    barrier.srcState = RHIResourceState::Undefined;
    barrier.dstState = RHIResourceState::CopyDst;
    barrier.baseMipLevel = 0;
    barrier.mipLevelCount = 1;
    barrier.baseArrayLayer = 0;
    barrier.arrayLayerCount = 1;
    rhi->cmdPipelineBarrier(cmd, {}, std::span(&barrier, 1));

    // Copy staging -> texture
    rhi->cmdCopyBufferToTexture(cmd, stagingBuffer, texture->m_texture, 0, 0, 0);

    // Transition to ShaderResource
    barrier.srcState = RHIResourceState::CopyDst;
    barrier.dstState = RHIResourceState::ShaderResource;
    rhi->cmdPipelineBarrier(cmd, {}, std::span(&barrier, 1));

    rhi->endCommandBuffer(cmd);

    // Submit and wait
    auto queue = rhi->getQueue(RHIQueueType::Graphics);
    auto fence = rhi->createFence(false);
    RHI::SubmitInfo submitInfo{};
    submitInfo.commandBuffers = std::span(&cmd, 1);
    submitInfo.fence = fence;
    rhi->queueSubmit(queue, submitInfo);
    rhi->waitForFence(fence);

    // Cleanup temporary resources
    rhi->destroyFence(fence);
    rhi->freeCommandBuffer(cmdPool, cmd);
    rhi->destroyCommandPool(cmdPool);
    rhi->destroyBuffer(stagingBuffer);

    // 4. Create sampler
    RHISamplerDesc samplerDesc{};
    samplerDesc.magFilter = RHIFilter::Linear;
    samplerDesc.minFilter = RHIFilter::Linear;
    samplerDesc.mipmapMode = RHIFilter::Linear;
    samplerDesc.addressModeU = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeV = RHISamplerAddressMode::Repeat;
    samplerDesc.addressModeW = RHISamplerAddressMode::Repeat;
    samplerDesc.anisotropyEnable = true;
    samplerDesc.maxAnisotropy = 16.0f;

    texture->m_sampler = rhi->createSampler(samplerDesc);
    if (!texture->m_sampler)
    {
        LOG_ERROR("Texture::create: Failed to create sampler");
        return nullptr;
    }

    LOG_DEBUG("Texture::create: Created '{}' ({}x{}, {})",
              debugName ? debugName : "unnamed",
              data.width, data.height,
              data.isSRGB ? "sRGB" : "linear");

    return texture;
}

std::shared_ptr<Texture> Texture::createFromHandles(
    RHI* rhi,
    RHITextureHandle textureHandle,
    RHISamplerHandle samplerHandle,
    uint32_t width,
    uint32_t height,
    RHIFormat format)
{
    if (!rhi || !textureHandle || !samplerHandle)
    {
        LOG_ERROR("Texture::createFromHandles: Invalid parameters");
        return nullptr;
    }

    auto texture = std::make_shared<Texture>();
    texture->m_rhi = rhi;
    texture->m_texture = textureHandle;
    texture->m_sampler = samplerHandle;
    texture->m_width = width;
    texture->m_height = height;
    texture->m_format = format;

    return texture;
}

} // namespace vesper
