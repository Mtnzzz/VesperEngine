# ==============================================================================
# VesperEngine - Shader Compilation (Slang)
# ==============================================================================

set(TARGET_NAME ${SHADER_COMPILE_TARGET})

# ==============================================================================
# Collect Shader Files
# ==============================================================================
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/slang/*.slang"
)

# Organize shaders in IDE
source_group("shaders" FILES ${SHADER_FILES})

# ==============================================================================
# Shader Include Directory
# ==============================================================================
set(SHADER_INCLUDE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/slang/include)

# ==============================================================================
# Output Directories
# ==============================================================================
set(GENERATED_SHADER_FOLDER "generated")

# Create output directories if they don't exist
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_SHADER_FOLDER})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_SHADER_FOLDER})
endif()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_SHADER_FOLDER}/spv)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_SHADER_FOLDER}/spv)
endif()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_SHADER_FOLDER}/cpp)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATED_SHADER_FOLDER}/cpp)
endif()

# ==============================================================================
# Shader Compilation
# ==============================================================================
include(${VESPER_ROOT_DIR}/cmake/ShaderCompile.cmake)

compile_slang_shader(
    "${SHADER_FILES}"
    "${TARGET_NAME}"
    "${SHADER_INCLUDE_FOLDER}"
    "${GENERATED_SHADER_FOLDER}"
    "${SLANG_COMPILER}"
)

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Engine")