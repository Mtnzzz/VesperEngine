# ==============================================================================
# VesperEngine - Shader Compilation CMake Module
# ==============================================================================
#
# Usage:
#   compile_slang_shader(
#       <shader_files>
#       <target_name>
#       <include_folder>
#       <output_folder>
#       <slang_compiler_executable>
#   )
#
# This module compiles Slang shaders to SPIR-V and optionally generates C++ headers
# with embedded shader bytecode.

function(compile_slang_shader SHADER_FILES TARGET_NAME INCLUDE_FOLDER OUTPUT_FOLDER SLANG_COMPILER)

    # Get the source directory (where shaders are located)
    set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

    # Output directories
    set(SPV_OUTPUT_DIR ${SHADER_SOURCE_DIR}/${OUTPUT_FOLDER}/spv)
    set(CPP_OUTPUT_DIR ${SHADER_SOURCE_DIR}/${OUTPUT_FOLDER}/cpp)

    # Create custom target for shader compilation
    add_custom_target(${TARGET_NAME})

    foreach(SHADER ${SHADER_FILES})
        # Get shader file name without extension
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        get_filename_component(SHADER_FULL_NAME ${SHADER} NAME)

        # Skip include files (modules)
        if(${SHADER} MATCHES ".*/include/.*")
            continue()
        endif()

        # Output SPIR-V files for vertex and fragment stages
        set(SPV_OUTPUT_VERT ${SPV_OUTPUT_DIR}/${SHADER_NAME}.vert.spv)
        set(SPV_OUTPUT_FRAG ${SPV_OUTPUT_DIR}/${SHADER_NAME}.frag.spv)

        # Generate header name for C++ embedding (optional)
        string(REPLACE "." "_" HEADER_NAME ${SHADER_NAME})
        string(TOUPPER ${HEADER_NAME} HEADER_VAR)

        # Add compilation command for vertex shader
        add_custom_command(
            OUTPUT ${SPV_OUTPUT_VERT}
            COMMAND ${SLANG_COMPILER}
                -I ${INCLUDE_FOLDER}
                -target spirv
                -profile glsl_450
                -entry vertexMain
                -stage vertex
                -o ${SPV_OUTPUT_VERT}
                ${SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling Slang vertex shader: ${SHADER_FULL_NAME}"
            VERBATIM
        )

        # Add compilation command for fragment shader
        add_custom_command(
            OUTPUT ${SPV_OUTPUT_FRAG}
            COMMAND ${SLANG_COMPILER}
                -I ${INCLUDE_FOLDER}
                -target spirv
                -profile glsl_450
                -entry fragmentMain
                -stage fragment
                -o ${SPV_OUTPUT_FRAG}
                ${SHADER}
            DEPENDS ${SHADER}
            COMMENT "Compiling Slang fragment shader: ${SHADER_FULL_NAME}"
            VERBATIM
        )

        # Add to target dependencies
        add_custom_target(${SHADER_NAME}_vert_spv DEPENDS ${SPV_OUTPUT_VERT})
        add_custom_target(${SHADER_NAME}_frag_spv DEPENDS ${SPV_OUTPUT_FRAG})
        add_dependencies(${TARGET_NAME} ${SHADER_NAME}_vert_spv ${SHADER_NAME}_frag_spv)

        # Set folder for IDE organization
        set_target_properties(${SHADER_NAME}_vert_spv PROPERTIES FOLDER "Shaders")
        set_target_properties(${SHADER_NAME}_frag_spv PROPERTIES FOLDER "Shaders")

    endforeach()

endfunction()

# ==============================================================================
# Helper function to embed SPIR-V into C++ header
# ==============================================================================
function(embed_spirv_to_header SPV_FILE HEADER_FILE VAR_NAME)

    # Read the SPIR-V binary
    file(READ ${SPV_FILE} SPV_CONTENT HEX)

    # Convert to C array format
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " SPV_ARRAY ${SPV_CONTENT})

    # Get file size
    file(SIZE ${SPV_FILE} SPV_SIZE)

    # Generate header content
    set(HEADER_CONTENT
"#pragma once
// Auto-generated shader header
// Source: ${SPV_FILE}

#include <cstdint>
#include <array>

namespace vesper::shaders {

inline constexpr std::array<uint8_t, ${SPV_SIZE}> ${VAR_NAME} = {
    ${SPV_ARRAY}
};

} // namespace vesper::shaders
")

    # Write header file
    file(WRITE ${HEADER_FILE} "${HEADER_CONTENT}")

endfunction()