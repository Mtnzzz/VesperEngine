# ==============================================================================
# VesperEngine - CMake Utility Functions
# ==============================================================================

# ==============================================================================
# Print a styled status message
# ==============================================================================
function(vesper_status MESSAGE)
    message(STATUS "[Vesper] ${MESSAGE}")
endfunction()

# ==============================================================================
# Set target folder for Visual Studio organization
# ==============================================================================
function(vesper_set_folder TARGET FOLDER)
    set_target_properties(${TARGET} PROPERTIES FOLDER "${FOLDER}")
endfunction()

# ==============================================================================
# Add a module library with standard settings
# ==============================================================================
function(vesper_add_module MODULE_NAME)
    set(options INTERFACE)
    set(oneValueArgs FOLDER)
    set(multiValueArgs SOURCES HEADERS DEPENDENCIES INCLUDE_DIRS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_INTERFACE)
        add_library(${MODULE_NAME} INTERFACE)
        target_include_directories(${MODULE_NAME} INTERFACE ${ARG_INCLUDE_DIRS})
    else()
        add_library(${MODULE_NAME} STATIC ${ARG_SOURCES} ${ARG_HEADERS})
        target_include_directories(${MODULE_NAME} PUBLIC ${ARG_INCLUDE_DIRS})

        set_target_properties(${MODULE_NAME} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
        )
    endif()

    if(ARG_DEPENDENCIES)
        target_link_libraries(${MODULE_NAME} PUBLIC ${ARG_DEPENDENCIES})
    endif()

    if(ARG_FOLDER)
        set_target_properties(${MODULE_NAME} PROPERTIES FOLDER "${ARG_FOLDER}")
    endif()
endfunction()

# ==============================================================================
# Group source files for IDE
# ==============================================================================
function(vesper_group_sources TARGET)
    get_target_property(SOURCES ${TARGET} SOURCES)
    get_target_property(SOURCE_DIR ${TARGET} SOURCE_DIR)

    foreach(SOURCE ${SOURCES})
        get_filename_component(SOURCE_PATH "${SOURCE}" PATH)
        file(RELATIVE_PATH SOURCE_PATH_REL "${SOURCE_DIR}" "${SOURCE_PATH}")
        string(REPLACE "/" "\\" GROUP_PATH "${SOURCE_PATH_REL}")
        source_group("${GROUP_PATH}" FILES "${SOURCE}")
    endforeach()
endfunction()

# ==============================================================================
# Copy files post-build
# ==============================================================================
function(vesper_copy_files TARGET)
    set(multiValueArgs FILES DESTINATION)
    cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

    foreach(FILE ${ARG_FILES})
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FILE}"
                "${ARG_DESTINATION}"
            COMMENT "Copying ${FILE} to ${ARG_DESTINATION}"
        )
    endforeach()
endfunction()

# ==============================================================================
# Platform detection macros
# ==============================================================================
macro(vesper_detect_platform)
    if(WIN32)
        set(VESPER_PLATFORM_WINDOWS TRUE)
        add_compile_definitions(VESPER_PLATFORM_WINDOWS)
    elseif(APPLE)
        set(VESPER_PLATFORM_MACOS TRUE)
        add_compile_definitions(VESPER_PLATFORM_MACOS)
    elseif(UNIX)
        set(VESPER_PLATFORM_LINUX TRUE)
        add_compile_definitions(VESPER_PLATFORM_LINUX)
    endif()
endmacro()

# ==============================================================================
# Build type configuration
# ==============================================================================
macro(vesper_configure_build_type)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(VESPER_DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_definitions(VESPER_RELEASE)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_definitions(VESPER_RELEASE VESPER_DEBUG_INFO)
    endif()
endmacro()