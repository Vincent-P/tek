#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

struct ImDrawVert
{
    vec2 pos;
    vec2 uv;
    uint col;
};

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer ImGuiVerts
{
	ImDrawVert vertices[];
};

layout(push_constant) uniform uPushConstant {
    vec2 scale;
    vec2 translate;
    ImGuiVerts vbuffer;
} c_;

layout(location = 0) out struct {
    vec4 color;
    vec2 uv;
} g_out;

void main() 
{
    ImDrawVert vertex = c_.vbuffer.vertices[gl_VertexIndex];
    
    g_out.color = unpackUnorm4x8(vertex.col);
    g_out.uv = vertex.uv;
    gl_Position = vec4(vertex.pos * c_.scale + c_.translate, 0.0, 1.0);
}