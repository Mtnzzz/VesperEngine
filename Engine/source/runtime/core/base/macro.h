#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define VESPER_PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #define VESPER_PLATFORM_MACOS
#elif defined(__linux__)
    #define VESPER_PLATFORM_LINUX
#endif

// Debug macros
#ifdef VESPER_DEBUG
    #define VESPER_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                /* TODO: Implement assertion */ \
            } \
        } while (false)
#else
    #define VESPER_ASSERT(condition, message) ((void)0)
#endif

// Export/Import macros
#ifdef VESPER_PLATFORM_WINDOWS
    #ifdef VESPER_BUILD_DLL
        #define VESPER_API __declspec(dllexport)
    #else
        #define VESPER_API __declspec(dllimport)
    #endif
#else
    #define VESPER_API
#endif

// Utility macros
#define VESPER_STRINGIFY(x) #x
#define VESPER_CONCAT(a, b) a##b

// Disable copy
#define VESPER_DISABLE_COPY(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete;

// Disable move
#define VESPER_DISABLE_MOVE(ClassName) \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete;

// Disable copy and move
#define VESPER_DISABLE_COPY_AND_MOVE(ClassName) \
    VESPER_DISABLE_COPY(ClassName) \
    VESPER_DISABLE_MOVE(ClassName)