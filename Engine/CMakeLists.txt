# ==============================================================================
# VesperEngine - Engine Module
# ==============================================================================

set(ENGINE_ROOT_DIR "${VESPER_ROOT_DIR}/Engine")
set(THIRD_PARTY_DIR "${ENGINE_ROOT_DIR}/3rdparty")
set(ENGINE_ASSET_DIR "/asset")

# Config directories
set(DEPLOY_CONFIG_DIR "configs/deployment")
set(DEVELOP_CONFIG_DIR "configs/development")

# ==============================================================================
# Build Options
# ==============================================================================
option(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER "Enable Physics Debug Renderer" OFF)
option(VESPER_ENABLE_VALIDATION_LAYERS "Enable Vulkan Validation Layers" ON)
option(VESPER_ENABLE_MULTITHREADED_RENDERING "Enable Multithreaded Rendering" ON)

# Physics debug renderer only supported on Windows
if(NOT WIN32)
    if(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
        message(WARNING "Physics Debug Renderer only supported on Windows. Disabling.")
        set(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER OFF CACHE BOOL "" FORCE)
    endif()
endif()

# ==============================================================================
# Compiler Settings
# ==============================================================================
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options("/MP")  # Multi-processor compilation
    add_compile_options("/utf-8")  # UTF-8 source and execution character set
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VesperEditor)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# Vulkan SDK Configuration
# ==============================================================================
# Use bundled Vulkan SDK from 3rdparty/vulkan
# The vulkan CMakeLists.txt creates interface targets and finds compilers
message(STATUS "Using bundled Vulkan SDK from 3rdparty/vulkan")

# Note: SLANG_COMPILER and GLSLANG_VALIDATOR are set by 3rdparty/vulkan/CMakeLists.txt
# after add_subdirectory(3rdparty) is called

# ==============================================================================
# Compile Definitions
# ==============================================================================
if(VESPER_ENABLE_VALIDATION_LAYERS)
    add_compile_definitions(VESPER_ENABLE_VALIDATION_LAYERS)
endif()

if(VESPER_ENABLE_MULTITHREADED_RENDERING)
    add_compile_definitions(VESPER_ENABLE_MULTITHREADED_RENDERING)
endif()

if(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
    add_compile_definitions(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
    set(JOLT_ASSET_DIR "/jolt-asset")
endif()

# ==============================================================================
# Third-Party Libraries (must be first to set SLANG_COMPILER)
# ==============================================================================
add_subdirectory(3rdparty)

# ==============================================================================
# Shader Compilation Target
# ==============================================================================
set(SHADER_COMPILE_TARGET VesperShaderCompile)
add_subdirectory(shader)

# ==============================================================================
# Engine Source Modules
# ==============================================================================
add_subdirectory(source/runtime)
add_subdirectory(source/editor)

# ==============================================================================
# Dependencies
# ==============================================================================
add_dependencies(VesperRuntime ${SHADER_COMPILE_TARGET})
