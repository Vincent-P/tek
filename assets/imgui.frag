#version 450
#include "bindless.h"

#define IMGUI_TEXTURE_INPUT 0

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec4 color;
    vec2 uv;
} g_in;


// https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/ColorSpaceUtility.hlsli
vec3 RemoveSRGBCurve(vec3 sRGB)
{
    // Approximately pow(x, 2.2)
    vec3 LinearLo = sRGB / 12.92;
    vec3 LinearHi = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
    bvec3 Mask = lessThan(sRGB, vec3(0.04045));
    vec3 Linear = mix(LinearHi, LinearLo, Mask);
    return Linear;
}

void main()
{
    vec4 color = g_in.color * texture(global_textures[IMGUI_TEXTURE_INPUT], g_in.uv);
    color.rgb = RemoveSRGBCurve(color.rgb);
    color.rgb *= color.a; // apply occlusion
    outColor = color;
}