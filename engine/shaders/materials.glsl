// Material and texture functions
// Requires: common.glsl

#ifndef MATERIALS_GLSL
#define MATERIALS_GLSL

// Procedural texture functions
vec3 checkerboardTexture(vec2 uv, vec3 color1, vec3 color2, float scale) {
    vec2 scaled = uv * scale;
    float checker = mod(floor(scaled.x) + floor(scaled.y), 2.0);
    return mix(color1, color2, checker);
}

vec3 stripedTexture(vec2 uv, vec3 color1, vec3 color2, float scale) {
    float stripe = sin(uv.x * scale * PI * 2.0) * 0.5 + 0.5;
    return mix(color1, color2, step(0.5, stripe));
}

vec3 gradientTexture(vec2 uv, vec3 color1, vec3 color2) {
    return mix(color1, color2, uv.y);
}

vec3 diagonalStripesTexture(vec2 uv, vec3 color1, vec3 color2, float scale) {
    float diag = sin((uv.x + uv.y) * scale) * 0.5 + 0.5;
    return mix(color1, color2, step(0.5, diag));
}

// Sample procedural texture based on pattern index
vec3 sampleProceduralTexture(vec2 uv, uint patternIndex) {
    uint pattern = patternIndex % 5;

    vec3 color1 = vec3(0.8, 0.7, 0.5);  // Light ceramic
    vec3 color2 = vec3(0.5, 0.4, 0.3);  // Dark ceramic

    if (pattern == 0) {
        return checkerboardTexture(uv, color1, color2, 8.0);
    } else if (pattern == 1) {
        return stripedTexture(uv, color1, color2, 6.0);
    } else if (pattern == 2) {
        return gradientTexture(uv, color1, color2);
    } else if (pattern == 3) {
        return diagonalStripesTexture(uv, color1, color2, 20.0);
    } else {
        // Solid with slight variation
        return color1 * (0.9 + fract(sin(float(patternIndex) * 43.758)) * 0.2);
    }
}

#endif // MATERIALS_GLSL
