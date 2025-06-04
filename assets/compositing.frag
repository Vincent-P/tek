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

void main()
{
    vec4 hdr = texture(global_textures[HDR_TEXTURE_INPUT], g_in.uv);
    vec4 linear = texture(global_textures[LINEAR_TEXTURE_INPUT], g_in.uv);
    float occlusion = 1.0 - linear.a;
    vec4 color = vec4(occlusion * hdr.rgb + linear.rgb, 1.0);
    outColor = color;
}