# ==============================================================================
# VesperEngine - Vulkan SDK Interface Library
# ==============================================================================
# This creates an INTERFACE library that provides all Vulkan-related includes
# and libraries in a unified way.

# ==============================================================================
# Vulkan Core
# ==============================================================================
add_library(vesper_vulkan INTERFACE)

target_include_directories(vesper_vulkan INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/vulkan
)

# Link Vulkan library
if(WIN32)
    target_link_libraries(vesper_vulkan INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/vulkan-1.lib
    )
elseif(APPLE)
    target_link_libraries(vesper_vulkan INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libvulkan.1.dylib
    )
elseif(UNIX)
    target_link_libraries(vesper_vulkan INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libvulkan.so.1
    )
endif()

# ==============================================================================
# Slang Shader Compiler Support (with runtime library linking)
# ==============================================================================
add_library(vesper_slang INTERFACE)

target_include_directories(vesper_slang INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/slang
)

# Link Slang import library based on configuration
if(WIN32)
    target_link_libraries(vesper_slang INTERFACE
        $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/lib/slangd.lib>
        $<$<NOT:$<CONFIG:Debug>>:${CMAKE_CURRENT_SOURCE_DIR}/lib/slang.lib>
    )

    # Store DLL paths for later copying
    set(SLANG_DLL_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin/debug/slangd.dll" CACHE INTERNAL "")
    set(SLANG_DLL_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin/release/slang.dll" CACHE INTERNAL "")
endif()

# ==============================================================================
# SPIRV-Tools Support
# ==============================================================================
add_library(vesper_spirv_tools INTERFACE)

target_include_directories(vesper_spirv_tools INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/spirv-tools
)

# ==============================================================================
# SPIRV-Cross Support (now provided by Engine/3rdparty/SPIRV-Cross)
# ==============================================================================
# SPIRV-Cross is now built from source in Engine/3rdparty/SPIRV-Cross
# Use targets: spirv-cross-core, spirv-cross-cpp

# ==============================================================================
# SPIRV-Headers Support
# ==============================================================================
add_library(vesper_spirv_headers INTERFACE)

target_include_directories(vesper_spirv_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/spirv-headers
)

# ==============================================================================
# glslang Support
# ==============================================================================
add_library(vesper_glslang INTERFACE)

target_include_directories(vesper_glslang INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/glslang
)

# ==============================================================================
# DXC (DirectX Shader Compiler) Support
# ==============================================================================
add_library(vesper_dxc INTERFACE)

target_include_directories(vesper_dxc INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/dxc
)

# ==============================================================================
# Combined Vulkan Toolchain (all-in-one convenience target)
# ==============================================================================
add_library(vesper_vulkan_toolchain INTERFACE)

target_link_libraries(vesper_vulkan_toolchain INTERFACE
    vesper_vulkan
    vesper_slang
    vesper_spirv_tools
    vesper_spirv_headers
    vesper_glslang
    vesper_dxc
)

# ==============================================================================
# Find Slang Compiler Executable
# ==============================================================================
# Look for slangc in common locations
find_program(SLANG_COMPILER slangc
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/bin
        $ENV{SLANG_PATH}/bin
        $ENV{VULKAN_SDK}/Bin
    PATHS
        "C:/Program Files/slang/bin"
        "/usr/local/bin"
        "/usr/bin"
)

if(SLANG_COMPILER)
    message(STATUS "Found Slang compiler: ${SLANG_COMPILER}")
else()
    message(WARNING "Slang compiler (slangc) not found. Shader compilation may fail.")
    # Set a placeholder - user will need to provide the path
    set(SLANG_COMPILER "slangc" CACHE FILEPATH "Path to Slang compiler")
endif()

# Export SLANG_COMPILER to parent scope
set(SLANG_COMPILER ${SLANG_COMPILER} PARENT_SCOPE)

# ==============================================================================
# Find glslangValidator (for legacy GLSL support if needed)
# ==============================================================================
find_program(GLSLANG_VALIDATOR glslangValidator
    HINTS
        ${CMAKE_CURRENT_SOURCE_DIR}/bin
        $ENV{VULKAN_SDK}/Bin
    PATHS
        "C:/VulkanSDK/*/Bin"
        "/usr/local/bin"
        "/usr/bin"
)

if(GLSLANG_VALIDATOR)
    message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")
    set(GLSLANG_VALIDATOR ${GLSLANG_VALIDATOR} PARENT_SCOPE)
endif()