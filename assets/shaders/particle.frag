#version 450

layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Soft circular particle shape with smooth edge attenuation
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);
    if (distSq > 0.25) discard;

    float alpha = smoothstep(0.25, 0.0, distSq);
    outColor = vec4(inColor.rgb, inColor.a * alpha);
}