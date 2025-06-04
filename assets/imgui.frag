#version 450
#include "bindless.h"

#define IMGUI_TEXTURE_INPUT 0

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec4 color;
    vec2 uv;
} g_in;

void main()
{
    vec4 color = g_in.color * texture(global_textures[IMGUI_TEXTURE_INPUT], g_in.uv);
    color.rgb *= color.a;
    outColor = color;
}