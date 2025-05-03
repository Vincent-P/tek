#version 450

layout(set=0, binding=0) uniform sampler2D g_Textures[3];

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec2 uv;
} g_in;

void main()
{
    vec4 hdr = texture(g_Textures[1], g_in.uv);
    vec4 linear = texture(g_Textures[2], g_in.uv);
    float occlusion = 1.0 - linear.a;
    vec4 color = vec4(occlusion * hdr.rgb + linear.rgb, 1.0);
    outColor = color;
}