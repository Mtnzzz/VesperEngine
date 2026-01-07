# ==============================================================================
# VesperEngine - Third Party Libraries
# ==============================================================================

set(THIRD_PARTY_FOLDER "ThirdParty")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ==============================================================================
# Vulkan SDK and Related Tools (Slang, SPIRV-Tools, etc.)
# ==============================================================================
add_subdirectory(vulkan)

# ==============================================================================
# spdlog - Fast C++ logging library
# ==============================================================================
if(NOT TARGET spdlog)
    option(SPDLOG_BUILD_EXAMPLE "" OFF)
    option(SPDLOG_INSTALL "" OFF)
    add_subdirectory(spdlog)
    set_target_properties(spdlog PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/spdlog)
endif()

# ==============================================================================
# GLFW - Multi-platform library for OpenGL, OpenGL ES and Vulkan
# ==============================================================================
if(NOT TARGET glfw)
    option(GLFW_BUILD_EXAMPLES "" OFF)
    option(GLFW_BUILD_TESTS "" OFF)
    option(GLFW_BUILD_DOCS "" OFF)
    option(GLFW_INSTALL "" OFF)
    add_subdirectory(glfw)
    set_target_properties(glfw PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/glfw)
    if(TARGET update_mappings)
        set_target_properties(update_mappings PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/glfw)
    endif()
endif()

# ==============================================================================
# ImGui - Dear ImGui: Bloat-free Graphical User interface
# ==============================================================================
if(NOT TARGET imgui)
    include(imgui.cmake)
    set_target_properties(imgui PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/imgui)
endif()

# ==============================================================================
# stb - Single-file public domain libraries
# ==============================================================================
if(NOT TARGET stb)
    include(stb.cmake)
    set_target_properties(stb PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/stb)
endif()

# ==============================================================================
# tinyobjloader - Tiny but powerful single file wavefront obj loader
# ==============================================================================
if(NOT TARGET tinyobjloader)
    add_subdirectory(tinyobjloader)
    set_target_properties(tinyobjloader PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/tinyobjloader)
    if(TARGET uninstall)
        set_target_properties(uninstall PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/tinyobjloader)
    endif()
endif()

# ==============================================================================
# nlohmann/json - JSON for Modern C++
# ==============================================================================
if(NOT TARGET nlohmann_json)
    option(JSON_BuildTests "" OFF)
    option(JSON_Install "" OFF)
    add_subdirectory(json)
    set_target_properties(nlohmann_json PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/json)
endif()

# ==============================================================================
# Jolt Physics - A multi-core friendly physics engine
# ==============================================================================
if(NOT TARGET Jolt)
    option(TARGET_HELLO_WORLD "" OFF)
    option(TARGET_PERFORMANCE_TEST "" OFF)
    option(TARGET_SAMPLES "" OFF)
    option(TARGET_UNIT_TESTS "" OFF)
    option(TARGET_VIEWER "" OFF)

    if(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
        option(TARGET_TEST_FRAMEWORK "" ON)
    else()
        option(TARGET_TEST_FRAMEWORK "" OFF)
    endif()

    add_subdirectory(JoltPhysics/Build)

    # Clang compiler specific warning suppression
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-Wno-unqualified-std-cast-call" COMPILER_CHECK_UNQUALIFIED)
        if(COMPILER_CHECK_UNQUALIFIED)
            target_compile_options(Jolt PRIVATE "-Wno-unqualified-std-cast-call")
        endif()
    endif()

    # Set properties for Jolt
    if(VESPER_ENABLE_PHYSICS_DEBUG_RENDERER)
        set_target_properties(Jolt TestFramework
            PROPERTIES
            FOLDER ${THIRD_PARTY_FOLDER}/JoltPhysics
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    else()
        set_target_properties(Jolt
            PROPERTIES
            FOLDER ${THIRD_PARTY_FOLDER}/JoltPhysics
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    endif()

    if(MSVC)
        target_compile_options(Jolt PRIVATE "/W4")
    else()
        target_compile_options(Jolt PRIVATE "-Wall")
    endif()
endif()

# ==============================================================================
# Lua + Sol2 - Scripting support
# ==============================================================================
if(NOT TARGET lua_static)
    include(lua.cmake)
endif()

if(NOT TARGET sol2)
    add_subdirectory(sol2)
endif()

# ==============================================================================
# DirectXMath - SIMD C++ math library
# ==============================================================================
if(NOT TARGET DirectXMath)
    add_subdirectory(DirectXMath)
endif()

# ==============================================================================
# SPIRV-Cross - SPIR-V parsing and cross-compilation tools
# ==============================================================================
if(NOT TARGET spirv-cross-core)
    # Disable unnecessary features (use FORCE to override cached values)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)   # Required for CPP support
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

    add_subdirectory(SPIRV-Cross)

    # Set folder for IDE
    if(TARGET spirv-cross-core)
        set_target_properties(spirv-cross-core PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/SPIRV-Cross)
    endif()
    if(TARGET spirv-cross-glsl)
        set_target_properties(spirv-cross-glsl PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/SPIRV-Cross)
    endif()
    if(TARGET spirv-cross-cpp)
        set_target_properties(spirv-cross-cpp PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/SPIRV-Cross)
    endif()
endif()

# ==============================================================================
# Assimp - Open Asset Import Library
# ==============================================================================
if(NOT TARGET assimp)
    # Disable unnecessary features to speed up build
    option(ASSIMP_BUILD_TESTS "" OFF)
    option(ASSIMP_BUILD_SAMPLES "" OFF)
    option(ASSIMP_BUILD_DOCS "" OFF)
    option(ASSIMP_BUILD_ASSIMP_TOOLS "" OFF)
    option(ASSIMP_INSTALL "" OFF)
    option(ASSIMP_INSTALL_PDB "" OFF)
    option(ASSIMP_INJECT_DEBUG_POSTFIX "" OFF)
    option(ASSIMP_BUILD_ZLIB "" ON)

    # Build as static library
    option(BUILD_SHARED_LIBS "" OFF)
    option(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT "" OFF)

    # Enable only commonly used importers
    option(ASSIMP_BUILD_OBJ_IMPORTER "" ON)
    option(ASSIMP_BUILD_GLTF_IMPORTER "" ON)
    option(ASSIMP_BUILD_FBX_IMPORTER "" ON)
    option(ASSIMP_BUILD_BLEND_IMPORTER "" ON)

    # Disable warnings-as-errors for Assimp (has some unused parameter warnings)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

    add_subdirectory(assimp)

    set_target_properties(assimp PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/assimp)

    # Suppress warnings in Assimp for Clang/GCC (third-party code)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(assimp PRIVATE
            -Wno-unused-parameter
            -Wno-error=unused-parameter
        )
    endif()

    # Hide internal targets
    if(TARGET zlibstatic)
        set_target_properties(zlibstatic PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/assimp)
    endif()
    if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
        set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/assimp)
    endif()
endif()

# ==============================================================================
# EnTT - Entity Component System
# ==============================================================================
if(NOT TARGET EnTT)
    add_subdirectory(entt)
    set_target_properties(EnTT PROPERTIES FOLDER ${THIRD_PARTY_FOLDER}/entt)
endif()