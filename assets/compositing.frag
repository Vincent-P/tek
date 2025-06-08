#version 450
#include "bindless.h"

#define IMGUI_TEXTURE_INPUT 0
#define HDR_TEXTURE_INPUT 1
#define LINEAR_TEXTURE_INPUT 2
#define IMAGE_OUTPUT 0

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec2 uv;
} g_in;

vec3 saturate(vec3 f)
{
return clamp(f, vec3(0), vec3(1));
}

vec3 ACESFilm(vec3 x)
{
float a = 2.51f;
float b = 0.03f;
float c = 2.43f;
float d = 0.59f;
float e = 0.14f;
return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

void main()
{
    ivec2 pixel_coords = ivec2(g_in.uv * textureSize(global_textures_ms[HDR_TEXTURE_INPUT]));

    vec4 hdr0 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 0);
    vec4 hdr1 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 1);
    vec4 hdr2 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 2);
    vec4 hdr3 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 3);
    vec4 hdr = (hdr0 + hdr1 + hdr2 + hdr3) * 0.25;

    vec3 mapped_hdr = ACESFilm(hdr.rgb);

    vec4 linear = texture(global_textures[LINEAR_TEXTURE_INPUT], g_in.uv);
    float occlusion = 1.0 - linear.a;
    vec4 color = vec4(occlusion * mapped_hdr + linear.rgb, 1.0);

    outColor = color;
}