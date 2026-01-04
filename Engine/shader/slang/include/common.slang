// Common Slang includes for VesperEngine shaders

module common;

// Constants
static const float PI = 3.14159265359;
static const float TWO_PI = 6.28318530718;
static const float HALF_PI = 1.57079632679;
static const float INV_PI = 0.31830988618;

// Utility functions
float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

float3 saturate(float3 x) {
    return clamp(x, float3(0.0), float3(1.0));
}

// Linear to sRGB conversion
float3 linearToSRGB(float3 color) {
    return pow(color, float3(1.0 / 2.2));
}

// sRGB to Linear conversion
float3 sRGBToLinear(float3 color) {
    return pow(color, float3(2.2));
}