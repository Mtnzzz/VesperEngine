cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(VesperEngine VERSION 0.1.0 LANGUAGES C CXX)

# ==============================================================================
# Global Settings
# ==============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakeDependentOption)
include(FetchContent)

# ==============================================================================
# Build Options
# ==============================================================================
option(VESPER_BUILD_TESTS "Build unit tests" ON)

# ==============================================================================
# In-source Build Guard
# ==============================================================================
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

# ==============================================================================
# Project Paths
# ==============================================================================
set(VESPER_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INSTALL_PREFIX "${VESPER_ROOT_DIR}/bin")
set(BINARY_ROOT_DIR "${CMAKE_INSTALL_PREFIX}/")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# CMake Module Path
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${VESPER_ROOT_DIR}/cmake")

# ==============================================================================
# Copy compile_commands.json to source for IDE support
# ==============================================================================
if(EXISTS ${CMAKE_BINARY_DIR}/compile_commands.json)
    file(COPY ${CMAKE_BINARY_DIR}/compile_commands.json DESTINATION ${CMAKE_SOURCE_DIR})
endif()

# ==============================================================================
# Add Engine Subdirectory
# ==============================================================================
add_subdirectory(Engine)

# ==============================================================================
# Testing with GoogleTest
# ==============================================================================
if(VESPER_BUILD_TESTS)
    # Enable CTest
    enable_testing()

    # Fetch GoogleTest
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Disable installing gtest
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    # Add tests subdirectory
    add_subdirectory(tests)
endif()
