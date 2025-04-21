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

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4x3 view;
    DdGuiVerts vbuffer;
} c_;

layout(location = 0) out struct {
    vec4 color;
} g_out;

vec4 float34_mul(mat4x3 m, vec3 v)
{
	vec4 result;
	result.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0];
	result.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1];
	result.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2];
	result.w = 1.0;
	return result;
}

void main() 
{
    DdDrawVert vertex = c_.vbuffer.vertices[gl_VertexIndex];

    vec4 pos = c_.proj * float34_mul(c_.view, vertex.pos);
    
    g_out.color = unpackUnorm4x8(vertex.col);
    gl_Position = pos;
}