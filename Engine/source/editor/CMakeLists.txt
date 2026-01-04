# ==============================================================================
# VesperEngine - Editor Executable
# ==============================================================================

set(TARGET_NAME VesperEditor)

# ==============================================================================
# Source Files Collection
# ==============================================================================
file(GLOB EDITOR_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

file(GLOB EDITOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
)

# Windows resource file
if(WIN32)
    file(GLOB EDITOR_RESOURCE CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/resource/*.rc
    )
endif()

# Organize source files in IDE
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${EDITOR_HEADERS}
    ${EDITOR_SOURCES}
    ${EDITOR_RESOURCE}
)

# ==============================================================================
# Create Executable Target
# ==============================================================================
if(WIN32)
    add_executable(${TARGET_NAME} ${EDITOR_HEADERS} ${EDITOR_SOURCES} ${EDITOR_RESOURCE})
else()
    add_executable(${TARGET_NAME} ${EDITOR_HEADERS} ${EDITOR_SOURCES})
endif()

# Define root directory for runtime asset loading
add_compile_definitions("VESPER_ROOT_DIR=${BINARY_ROOT_DIR}")

# ==============================================================================
# Target Properties
# ==============================================================================
set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    OUTPUT_NAME "VesperEditor"
    FOLDER "Engine"
)

# ==============================================================================
# Compiler Options
# ==============================================================================
target_compile_options(${TARGET_NAME} PUBLIC
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX->"
)

# ==============================================================================
# Include Directories
# ==============================================================================
target_include_directories(${TARGET_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<BUILD_INTERFACE:${THIRD_PARTY_DIR}/stb>
)

# ==============================================================================
# Link Dependencies
# ==============================================================================
target_link_libraries(${TARGET_NAME} PRIVATE VesperRuntime)

# ==============================================================================
# Post-Build Commands - Copy Assets and Configs
# ==============================================================================
set(POST_BUILD_COMMANDS
    # Create binary directory
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINARY_ROOT_DIR}"

    # Copy editor resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resource"
        "${BINARY_ROOT_DIR}/resource"

    # Copy executable and dependencies
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
        "${BINARY_ROOT_DIR}"

    # Copy deployment config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ENGINE_ROOT_DIR}/${DEPLOY_CONFIG_DIR}/${TARGET_NAME}.ini"
        "${BINARY_ROOT_DIR}"

    # Copy development config to build directory
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ENGINE_ROOT_DIR}/${DEVELOP_CONFIG_DIR}/${TARGET_NAME}.ini"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/"

    # Copy engine assets
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${BINARY_ROOT_DIR}/${ENGINE_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ENGINE_ROOT_DIR}/${ENGINE_ASSET_DIR}"
        "${BINARY_ROOT_DIR}/${ENGINE_ASSET_DIR}"
)

# Physics debug assets (Windows only)
if(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
    set(POST_BUILD_COMMANDS ${POST_BUILD_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${BINARY_ROOT_DIR}/${JOLT_ASSET_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ENGINE_ROOT_DIR}/${JOLT_ASSET_DIR}"
            "${BINARY_ROOT_DIR}/${JOLT_ASSET_DIR}"
    )
endif()

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD ${POST_BUILD_COMMANDS})

# ==============================================================================
# Copy Slang DLL based on configuration
# ==============================================================================
if(WIN32 AND SLANG_DLL_DEBUG AND SLANG_DLL_RELEASE)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${SLANG_DLL_DEBUG},${SLANG_DLL_RELEASE}>"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
        COMMENT "Copying Slang DLL to output directory"
    )
endif()

# ==============================================================================
# Export Header Files for Precompile/Reflection
# ==============================================================================
set(VESPER_EDITOR_HEADERS "${EDITOR_HEADERS}" PARENT_SCOPE)