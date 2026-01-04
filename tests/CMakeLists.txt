# ==============================================================================
# VesperEngine - Unit Tests
# ==============================================================================

include(GoogleTest)

# ==============================================================================
# Test Executable
# ==============================================================================
set(TEST_TARGET VesperTests)

set(TEST_SOURCES
    test_main.cpp
    test_log_system.cpp
    test_math_vector.cpp
    test_math_quaternion.cpp
    test_math_matrix.cpp
    test_engine.cpp
    test_window_system.cpp
    test_input_system.cpp
)

add_executable(${TEST_TARGET} ${TEST_SOURCES})

target_link_libraries(${TEST_TARGET}
    PRIVATE
        VesperRuntime
        GTest::gtest
)

# Include paths
target_include_directories(${TEST_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${VESPER_ROOT_DIR}/Engine/source
        ${VESPER_ROOT_DIR}/Engine/source/runtime
)

# Set folder in IDE
set_target_properties(${TEST_TARGET} PROPERTIES
    FOLDER "Tests"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# ==============================================================================
# Register Tests with CTest
# ==============================================================================
gtest_discover_tests(${TEST_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    PROPERTIES
        LABELS "unit"
)
