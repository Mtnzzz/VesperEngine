# ==============================================================================
# VesperEngine - Runtime Library
# ==============================================================================

set(TARGET_NAME VesperRuntime)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Source Files Collection
# ==============================================================================
# Note: GLOB_RECURSE is used for convenience, but explicit listing is preferred
# for large projects. Consider using explicit file lists for better build reliability.

file(GLOB_RECURSE RUNTIME_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

file(GLOB_RECURSE RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# Organize source files in IDE
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${RUNTIME_HEADERS} ${RUNTIME_SOURCES})

# ==============================================================================
# Create Library Target
# ==============================================================================
add_library(${TARGET_NAME} STATIC ${RUNTIME_HEADERS} ${RUNTIME_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Engine"
)

# ==============================================================================
# Compiler Options
# ==============================================================================
# MSVC specific
target_compile_options(${TARGET_NAME} PUBLIC
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->"
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->"
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:preprocessor>"
)

# ==============================================================================
# Include Directories
# ==============================================================================

# Runtime source root
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>
        $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source/runtime>
        $<INSTALL_INTERFACE:include/${TARGET_NAME}-${PROJECT_VERSION}>
)

# Generated shader headers
target_include_directories(${TARGET_NAME}
    PUBLIC $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/shader/generated/cpp>
)

# ==============================================================================
# Link Dependencies
# ==============================================================================

# Logging
target_link_libraries(${TARGET_NAME} PUBLIC spdlog::spdlog)

# Windowing
target_link_libraries(${TARGET_NAME} PUBLIC glfw)

# GUI
target_link_libraries(${TARGET_NAME} PUBLIC imgui)

# Math (DirectXMath)
target_link_libraries(${TARGET_NAME} PUBLIC DirectXMath)

# Physics
target_link_libraries(${TARGET_NAME} PUBLIC Jolt)

# Scripting
target_link_libraries(${TARGET_NAME} PUBLIC lua_static sol2)

# Asset loading
target_link_libraries(${TARGET_NAME} PRIVATE tinyobjloader stb assimp)

# JSON parsing
target_link_libraries(${TARGET_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Vulkan and related tools (includes Vulkan, Slang, SPIRV-Tools, etc.)
target_link_libraries(${TARGET_NAME} PUBLIC vesper_vulkan_toolchain)

# Physics debug renderer (Windows only)
if(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
    target_link_libraries(${TARGET_NAME} PUBLIC TestFramework d3d12.lib shcore.lib)
endif()

# ==============================================================================
# Precompiled Headers (Optional)
# ==============================================================================
# Uncomment to enable PCH for faster compilation
# target_precompile_headers(${TARGET_NAME} PRIVATE
#     <vector>
#     <string>
#     <memory>
#     <unordered_map>
#     <functional>
# )

# ==============================================================================
# Export Header Files for Precompile/Reflection
# ==============================================================================
set(VESPER_RUNTIME_HEADERS "${RUNTIME_HEADERS}" PARENT_SCOPE)