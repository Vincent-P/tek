#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

struct Drawer2DVert
{
    vec2 pos;
    uint col;
};

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer Drawer2DVerts
{
	Drawer2DVert vertices[];
};

layout(push_constant) uniform uPushConstant {
    vec2 scale;
    vec2 translate;
    Drawer2DVerts vbuffer;
} c_;

layout(location = 0) out struct {
    vec4 color;
} g_out;

void main() 
{
    Drawer2DVert vertex = c_.vbuffer.vertices[gl_VertexIndex];
    g_out.color = unpackUnorm4x8(vertex.col);
    gl_Position = vec4(vertex.pos * c_.scale + c_.translate, 0.0, 1.0);
}