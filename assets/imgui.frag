#version 450

layout(set=0, binding=0) uniform sampler2D FontAtlas;

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec4 color;
    vec2 uv;
} g_in;

void main()
{
    vec4 color = g_in.color * texture(FontAtlas, g_in.uv);
    color.rgb *= color.a;
    outColor = color;
}