#pragma once

#include "runtime/core/base/macro.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace vesper {

// ============================================================================
// Forward Declarations
// ============================================================================

struct RHIBuffer;
struct RHITexture;
struct RHISampler;
struct RHIShader;
struct RHIPipeline;
struct RHIDescriptorSet;
struct RHIDescriptorSetLayout;
struct RHICommandBuffer;
struct RHICommandPool;
struct RHIRenderPass;
struct RHIFramebuffer;
struct RHIFence;
struct RHISemaphore;
struct RHIQueue;
struct RHISwapChain;

// ============================================================================
// Handle Types (Opaque pointers for backend abstraction)
// ============================================================================

using RHIBufferHandle              = std::shared_ptr<RHIBuffer>;
using RHITextureHandle             = std::shared_ptr<RHITexture>;
using RHISamplerHandle             = std::shared_ptr<RHISampler>;
using RHIShaderHandle              = std::shared_ptr<RHIShader>;
using RHIPipelineHandle            = std::shared_ptr<RHIPipeline>;
using RHIDescriptorSetHandle       = std::shared_ptr<RHIDescriptorSet>;
using RHIDescriptorSetLayoutHandle = std::shared_ptr<RHIDescriptorSetLayout>;
using RHICommandBufferHandle       = std::shared_ptr<RHICommandBuffer>;
using RHICommandPoolHandle         = std::shared_ptr<RHICommandPool>;
using RHIRenderPassHandle          = std::shared_ptr<RHIRenderPass>;
using RHIFramebufferHandle         = std::shared_ptr<RHIFramebuffer>;
using RHIFenceHandle               = std::shared_ptr<RHIFence>;
using RHISemaphoreHandle           = std::shared_ptr<RHISemaphore>;
using RHIQueueHandle               = std::shared_ptr<RHIQueue>;
using RHISwapChainHandle           = std::shared_ptr<RHISwapChain>;

// ============================================================================
// Enums
// ============================================================================

enum class RHIBackendType : uint8_t
{
    Vulkan,
    D3D12,
    Metal,
};

enum class RHIQueueType : uint8_t
{
    Graphics,
    Compute,
    Transfer,
};

enum class RHIFormat : uint32_t
{
    Undefined = 0,

    // 8-bit formats
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,

    RG8_UNORM,
    RG8_SNORM,
    RG8_UINT,
    RG8_SINT,

    RGBA8_UNORM,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_SRGB,

    BGRA8_UNORM,
    BGRA8_SRGB,

    // 16-bit formats
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_FLOAT,

    RG16_UNORM,
    RG16_SNORM,
    RG16_UINT,
    RG16_SINT,
    RG16_FLOAT,

    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,

    // 32-bit formats
    R32_UINT,
    R32_SINT,
    R32_FLOAT,

    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,

    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,

    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,

    // Depth/Stencil formats
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,

    // Compressed formats
    BC1_RGB_UNORM,
    BC1_RGB_SRGB,
    BC1_RGBA_UNORM,
    BC1_RGBA_SRGB,
    BC2_UNORM,
    BC2_SRGB,
    BC3_UNORM,
    BC3_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_SRGB,
};

enum class RHIBufferUsage : uint32_t
{
    None         = 0,
    Vertex       = 1 << 0,
    Index        = 1 << 1,
    Uniform      = 1 << 2,
    Storage      = 1 << 3,
    Indirect     = 1 << 4,
    TransferSrc  = 1 << 5,
    TransferDst  = 1 << 6,
};

inline RHIBufferUsage operator|(RHIBufferUsage a, RHIBufferUsage b) {
    return static_cast<RHIBufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline RHIBufferUsage operator&(RHIBufferUsage a, RHIBufferUsage b) {
    return static_cast<RHIBufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasFlag(RHIBufferUsage flags, RHIBufferUsage flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

enum class RHITextureUsage : uint32_t
{
    None            = 0,
    Sampled         = 1 << 0,
    Storage         = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthStencil    = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
    InputAttachment = 1 << 6,
};

inline RHITextureUsage operator|(RHITextureUsage a, RHITextureUsage b) {
    return static_cast<RHITextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline RHITextureUsage operator&(RHITextureUsage a, RHITextureUsage b) {
    return static_cast<RHITextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasFlag(RHITextureUsage flags, RHITextureUsage flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

enum class RHIMemoryUsage : uint8_t
{
    GpuOnly,        // Device local, not host visible (textures, vertex buffers)
    CpuOnly,        // Host visible, cached (staging buffers)
    CpuToGpu,       // Host visible, coherent (uniform buffers)
    GpuToCpu,       // Device local, host visible (readback)
};

enum class RHIResourceState : uint32_t
{
    Undefined           = 0,
    Common              = 1 << 0,
    VertexBuffer        = 1 << 1,
    IndexBuffer         = 1 << 2,
    UniformBuffer       = 1 << 3,
    ShaderResource      = 1 << 4,
    UnorderedAccess     = 1 << 5,
    RenderTarget        = 1 << 6,
    DepthWrite          = 1 << 7,
    DepthRead           = 1 << 8,
    CopySrc             = 1 << 9,
    CopyDst             = 1 << 10,
    Present             = 1 << 11,
    IndirectArgument    = 1 << 12,
};

inline RHIResourceState operator|(RHIResourceState a, RHIResourceState b) {
    return static_cast<RHIResourceState>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

enum class RHITextureDimension : uint8_t
{
    Tex1D,
    Tex2D,
    Tex3D,
    TexCube,
    Tex1DArray,
    Tex2DArray,
    TexCubeArray,
};

enum class RHISampleCount : uint8_t
{
    Count1  = 1,
    Count2  = 2,
    Count4  = 4,
    Count8  = 8,
    Count16 = 16,
    Count32 = 32,
    Count64 = 64,
};

enum class RHILoadOp : uint8_t
{
    Load,
    Clear,
    DontCare,
};

enum class RHIStoreOp : uint8_t
{
    Store,
    DontCare,
};

enum class RHIShaderStage : uint32_t
{
    None     = 0,
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    Geometry = 1 << 3,
    Hull     = 1 << 4,
    Domain   = 1 << 5,
    All      = Vertex | Fragment | Compute | Geometry | Hull | Domain,
};

inline RHIShaderStage operator|(RHIShaderStage a, RHIShaderStage b) {
    return static_cast<RHIShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

enum class RHIPrimitiveTopology : uint8_t
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    PatchList,
};

enum class RHIPolygonMode : uint8_t
{
    Fill,
    Line,
    Point,
};

enum class RHICullMode : uint8_t
{
    None,
    Front,
    Back,
};

enum class RHIFrontFace : uint8_t
{
    CounterClockwise,
    Clockwise,
};

enum class RHICompareOp : uint8_t
{
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

enum class RHIStencilOp : uint8_t
{
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap,
};

enum class RHIBlendFactor : uint8_t
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    SrcAlphaSaturate,
};

enum class RHIBlendOp : uint8_t
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class RHIFilter : uint8_t
{
    Nearest,
    Linear,
};

enum class RHISamplerAddressMode : uint8_t
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

enum class RHIBorderColor : uint8_t
{
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
};

enum class RHIPresentMode : uint8_t
{
    Immediate,      // No vsync
    Mailbox,        // Triple buffering
    Fifo,           // Vsync
    FifoRelaxed,    // Vsync with tearing
};

enum class RHIDescriptorType : uint8_t
{
    Sampler,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformTexelBuffer,
    StorageTexelBuffer,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    InputAttachment,
};

enum class RHIVertexInputRate : uint8_t
{
    Vertex,
    Instance,
};

// ============================================================================
// Descriptor Structures
// ============================================================================

struct RHIExtent2D
{
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct RHIExtent3D
{
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t depth  = 1;
};

struct RHIOffset2D
{
    int32_t x = 0;
    int32_t y = 0;
};

struct RHIOffset3D
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};

struct RHIRect2D
{
    RHIOffset2D offset;
    RHIExtent2D extent;
};

struct RHIViewport
{
    float x        = 0.0f;
    float y        = 0.0f;
    float width    = 0.0f;
    float height   = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct RHIClearValue
{
    union {
        float    color[4];
        struct {
            float depth;
            uint32_t stencil;
        } depthStencil;
    };

    static RHIClearValue Color(float r, float g, float b, float a) {
        RHIClearValue v{};
        v.color[0] = r; v.color[1] = g; v.color[2] = b; v.color[3] = a;
        return v;
    }

    static RHIClearValue DepthStencil(float depth, uint32_t stencil = 0) {
        RHIClearValue v{};
        v.depthStencil.depth = depth;
        v.depthStencil.stencil = stencil;
        return v;
    }
};

// ============================================================================
// Creation Descriptors
// ============================================================================

struct RHIBufferDesc
{
    uint64_t        size        = 0;
    RHIBufferUsage  usage       = RHIBufferUsage::None;
    RHIMemoryUsage  memoryUsage = RHIMemoryUsage::GpuOnly;
    const char*     debugName   = nullptr;
};

struct RHITextureDesc
{
    RHIExtent3D         extent      = {1, 1, 1};
    uint32_t            mipLevels   = 1;
    uint32_t            arrayLayers = 1;
    RHIFormat           format      = RHIFormat::RGBA8_UNORM;
    RHITextureDimension dimension   = RHITextureDimension::Tex2D;
    RHISampleCount      sampleCount = RHISampleCount::Count1;
    RHITextureUsage     usage       = RHITextureUsage::Sampled;
    RHIMemoryUsage      memoryUsage = RHIMemoryUsage::GpuOnly;
    RHIResourceState    initialState = RHIResourceState::Undefined;
    const char*         debugName   = nullptr;
};

struct RHISamplerDesc
{
    RHIFilter             magFilter      = RHIFilter::Linear;
    RHIFilter             minFilter      = RHIFilter::Linear;
    RHIFilter             mipmapMode     = RHIFilter::Linear;
    RHISamplerAddressMode addressModeU   = RHISamplerAddressMode::Repeat;
    RHISamplerAddressMode addressModeV   = RHISamplerAddressMode::Repeat;
    RHISamplerAddressMode addressModeW   = RHISamplerAddressMode::Repeat;
    float                 mipLodBias     = 0.0f;
    bool                  anisotropyEnable = true;
    float                 maxAnisotropy  = 16.0f;
    bool                  compareEnable  = false;
    RHICompareOp          compareOp      = RHICompareOp::Always;
    float                 minLod         = 0.0f;
    float                 maxLod         = 1000.0f;
    RHIBorderColor        borderColor    = RHIBorderColor::OpaqueBlack;
    const char*           debugName      = nullptr;
};

struct RHIShaderDesc
{
    const uint8_t*  code     = nullptr;
    size_t          codeSize = 0;
    RHIShaderStage  stage    = RHIShaderStage::None;
    const char*     entryPoint = "main";
    const char*     debugName = nullptr;
};

struct RHISwapChainDesc
{
    void*           windowHandle = nullptr;
    uint32_t        width        = 0;
    uint32_t        height       = 0;
    uint32_t        imageCount   = 3;
    RHIFormat       format       = RHIFormat::BGRA8_SRGB;
    RHIPresentMode  presentMode  = RHIPresentMode::Fifo;
    bool            vsync        = true;
    const char*     debugName    = nullptr;
};

struct RHICommandPoolDesc
{
    RHIQueueType    queueType     = RHIQueueType::Graphics;
    bool            transient     = false;
    bool            resetCommands = true;
    const char*     debugName     = nullptr;
};

struct RHICommandBufferDesc
{
    bool        secondary = false;
    const char* debugName = nullptr;
};

// ============================================================================
// Vertex Input
// ============================================================================

struct RHIVertexBinding
{
    uint32_t            binding   = 0;
    uint32_t            stride    = 0;
    RHIVertexInputRate  inputRate = RHIVertexInputRate::Vertex;
};

struct RHIVertexAttribute
{
    uint32_t    location = 0;
    uint32_t    binding  = 0;
    RHIFormat   format   = RHIFormat::RGB32_FLOAT;
    uint32_t    offset   = 0;
};

struct RHIVertexInputState
{
    std::vector<RHIVertexBinding>   bindings;
    std::vector<RHIVertexAttribute> attributes;
};

// ============================================================================
// Pipeline State
// ============================================================================

struct RHIRasterizationState
{
    bool            depthClampEnable        = false;
    bool            rasterizerDiscardEnable = false;
    RHIPolygonMode  polygonMode             = RHIPolygonMode::Fill;
    RHICullMode     cullMode                = RHICullMode::Back;
    RHIFrontFace    frontFace               = RHIFrontFace::CounterClockwise;
    bool            depthBiasEnable         = false;
    float           depthBiasConstant       = 0.0f;
    float           depthBiasClamp          = 0.0f;
    float           depthBiasSlope          = 0.0f;
    float           lineWidth               = 1.0f;
};

struct RHIDepthStencilState
{
    bool            depthTestEnable     = true;
    bool            depthWriteEnable    = true;
    RHICompareOp    depthCompareOp      = RHICompareOp::Less;
    bool            depthBoundsEnable   = false;
    bool            stencilTestEnable   = false;
    float           minDepthBounds      = 0.0f;
    float           maxDepthBounds      = 1.0f;
};

struct RHIColorBlendAttachment
{
    bool            blendEnable     = false;
    RHIBlendFactor  srcColorFactor  = RHIBlendFactor::One;
    RHIBlendFactor  dstColorFactor  = RHIBlendFactor::Zero;
    RHIBlendOp      colorBlendOp    = RHIBlendOp::Add;
    RHIBlendFactor  srcAlphaFactor  = RHIBlendFactor::One;
    RHIBlendFactor  dstAlphaFactor  = RHIBlendFactor::Zero;
    RHIBlendOp      alphaBlendOp    = RHIBlendOp::Add;
    uint8_t         colorWriteMask  = 0xF; // R|G|B|A
};

struct RHIColorBlendState
{
    bool                                logicOpEnable   = false;
    std::vector<RHIColorBlendAttachment> attachments;
    float                               blendConstants[4] = {0, 0, 0, 0};
};

struct RHIMultisampleState
{
    RHISampleCount  sampleCount         = RHISampleCount::Count1;
    bool            sampleShadingEnable = false;
    float           minSampleShading    = 1.0f;
    bool            alphaToCoverageEnable = false;
    bool            alphaToOneEnable    = false;
};

// ============================================================================
// Render Pass Attachments (for dynamic rendering)
// ============================================================================

struct RHIRenderingAttachmentInfo
{
    RHITextureHandle    texture;
    RHILoadOp           loadOp     = RHILoadOp::Clear;
    RHIStoreOp          storeOp    = RHIStoreOp::Store;
    RHIClearValue       clearValue;
    RHITextureHandle    resolveTexture;  // For MSAA resolve
};

struct RHIRenderingInfo
{
    RHIRect2D                               renderArea;
    uint32_t                                layerCount = 1;
    std::vector<RHIRenderingAttachmentInfo> colorAttachments;
    RHIRenderingAttachmentInfo*             depthAttachment   = nullptr;
    RHIRenderingAttachmentInfo*             stencilAttachment = nullptr;
};

// ============================================================================
// Descriptor Set Binding
// ============================================================================

struct RHIDescriptorBinding
{
    uint32_t            binding         = 0;
    RHIDescriptorType   descriptorType  = RHIDescriptorType::UniformBuffer;
    uint32_t            descriptorCount = 1;
    RHIShaderStage      stageFlags      = RHIShaderStage::All;
};

struct RHIDescriptorSetLayoutDesc
{
    std::vector<RHIDescriptorBinding> bindings;
    const char*                       debugName = nullptr;
};

struct RHIDescriptorWrite
{
    uint32_t            binding     = 0;
    uint32_t            arrayElement = 0;
    RHIDescriptorType   type        = RHIDescriptorType::UniformBuffer;

    // One of these should be set based on type
    RHIBufferHandle     buffer;
    uint64_t            bufferOffset = 0;
    uint64_t            bufferRange  = 0;  // 0 = whole buffer

    RHITextureHandle    texture;
    RHISamplerHandle    sampler;
};

// ============================================================================
// Pipeline Descriptor
// ============================================================================

struct RHIPushConstantRange
{
    RHIShaderStage  stages = RHIShaderStage::Vertex;
    uint32_t        offset = 0;
    uint32_t        size   = 0;
};

struct RHIGraphicsPipelineDesc
{
    std::vector<RHIShaderHandle>            shaders;
    RHIVertexInputState                     vertexInput;
    RHIPrimitiveTopology                    topology        = RHIPrimitiveTopology::TriangleList;
    RHIRasterizationState                   rasterization;
    RHIMultisampleState                     multisample;
    RHIDepthStencilState                    depthStencil;
    RHIColorBlendState                      colorBlend;
    std::vector<RHIDescriptorSetLayoutHandle> descriptorLayouts;
    std::vector<RHIPushConstantRange>       pushConstantRanges;

    // Dynamic rendering formats
    std::vector<RHIFormat>                  colorFormats;
    RHIFormat                               depthFormat   = RHIFormat::Undefined;
    RHIFormat                               stencilFormat = RHIFormat::Undefined;

    const char*                             debugName = nullptr;
};

struct RHIComputePipelineDesc
{
    RHIShaderHandle                         shader;
    std::vector<RHIDescriptorSetLayoutHandle> descriptorLayouts;
    std::vector<RHIPushConstantRange>       pushConstantRanges;
    const char*                             debugName = nullptr;
};

// ============================================================================
// Barrier Structures
// ============================================================================

struct RHIBufferBarrier
{
    RHIBufferHandle     buffer;
    RHIResourceState    srcState;
    RHIResourceState    dstState;
    uint64_t            offset = 0;
    uint64_t            size   = 0;  // 0 = whole buffer
};

struct RHITextureBarrier
{
    RHITextureHandle    texture;
    RHIResourceState    srcState;
    RHIResourceState    dstState;
    uint32_t            baseMipLevel   = 0;
    uint32_t            mipLevelCount  = 1;
    uint32_t            baseArrayLayer = 0;
    uint32_t            arrayLayerCount = 1;
};

// ============================================================================
// GPU Information
// ============================================================================

struct RHIGpuInfo
{
    char        deviceName[256] = {};
    uint32_t    vendorId        = 0;
    uint32_t    deviceId        = 0;
    uint64_t    dedicatedMemory = 0;
    bool        discreteGpu     = false;

    // Capabilities
    bool        dynamicRendering            = false;
    bool        descriptorIndexing          = false;
    bool        bufferDeviceAddress         = false;
    bool        synchronization2            = false;
    bool        timelineSemaphore           = false;
    bool        raytracing                  = false;
    bool        meshShader                  = false;

    // Limits
    uint32_t    maxTextureSize              = 0;
    uint32_t    maxUniformBufferSize        = 0;
    uint32_t    maxStorageBufferSize        = 0;
    uint32_t    maxPushConstantSize         = 0;
    uint32_t    maxBoundDescriptorSets      = 0;
    uint32_t    maxColorAttachments         = 0;
    uint32_t    maxComputeWorkGroupCount[3] = {};
    uint32_t    maxComputeWorkGroupSize[3]  = {};
};

// ============================================================================
// RHI Initialization Config
// ============================================================================

struct RHIConfig
{
    const char*     applicationName     = "VesperEngine";
    uint32_t        applicationVersion  = 1;
    bool            enableValidation    = true;
    bool            enableGpuDebugMarkers = true;
    uint32_t        preferredGpuIndex   = 0;  // 0 = auto-select
};

} // namespace vesper
