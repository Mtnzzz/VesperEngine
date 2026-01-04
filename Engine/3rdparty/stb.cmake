# ==============================================================================
# stb CMake configuration (header-only library with implementation)
# ==============================================================================

set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb)

# Create implementation source file
set(STB_IMPL_FILE ${CMAKE_CURRENT_BINARY_DIR}/stb_impl.cpp)

file(WRITE ${STB_IMPL_FILE} "
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include \"stb_image.h\"
#include \"stb_image_write.h\"
#include \"stb_image_resize2.h\"
")

add_library(stb STATIC ${STB_IMPL_FILE})

target_include_directories(stb PUBLIC ${STB_DIR})