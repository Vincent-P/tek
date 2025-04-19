#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec4 color;
} g_in;

void main()
{
    vec4 color = g_in.color;
    color.rgb *= color.a;
    outColor = color;
}