#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

struct DdDrawVert
{
    vec3 pos;
    uint col;
};

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer DdGuiVerts
{
	DdDrawVert vertices[];
};

layout(push_constant) uniform uPushConstant {
    mat4 proj;
    mat4 view;
    DdGuiVerts vbuffer;
} c_;

layout(location = 0) out struct {
    vec4 color;
} g_out;

void main() 
{
    DdDrawVert vertex = c_.vbuffer.vertices[gl_VertexIndex];

    vec4 pos = (c_.proj * (c_.view * vec4(vertex.pos, 1.0)));
    
    g_out.color = unpackUnorm4x8(vertex.col);
    gl_Position = pos;
}