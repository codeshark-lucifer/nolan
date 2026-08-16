#version 450

layout(binding = 0) uniform sampler2D sceneTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979323846;
const float PI_HALF = 0.5 * PI;

vec3 applyVignette(vec3 sceneColor, vec2 uv) {
    // 1. Center of the vignette
    vec2 center = vec2(0.5, 0.5);

    // 2. Aspect ratio correction (Set to vec2(1.0) if you don't want 16:9 aspect scaling)
    vec2 aspectCorrection = vec2(16.0 / 9.0, 1.0);

    // 3. Adjustable Vignette Controls:
    float innerRadius = 0.5; // Unaffected center size (0.0 to 0.5)
    float radiusWidth = 0.65; // Softness width of the edge falloff

    float pihalfDivRadius = PI_HALF / radiusWidth;

    // 4. Calculate distance from center
    vec2 normalizedPos = (uv - center) * aspectCorrection;
    float dist = (length(normalizedPos) - innerRadius) * pihalfDivRadius;

    // 5. Cosine-squared (cos^2) camera lens falloff
    float linearFalloff = clamp(dist, 0.0, PI_HALF);
    float falloff = cos(linearFalloff) * cos(linearFalloff);

    return sceneColor * falloff;
}

void main() {
    vec3 color = texture(sceneTexture, inUV).rgb;

    // Apply Cosine-Squared Vignette
    color = applyVignette(color, inUV);

    outColor = vec4(color, 1.0);
}