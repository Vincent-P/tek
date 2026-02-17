#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer TransformBuffer
{
	mat4x3 matrices[];
};

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer MeshFloat3Buffer
{
	vec3 data[];
};

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4x3 view;
    mat4x3 invview;
    vec2 ibl_buffer;
    TransformBuffer instances_buffer;
    MeshFloat3Buffer positions_vbuffer;
    MeshFloat3Buffer last_positions_vbuffer;
    MeshFloat3Buffer normals_vbuffer;
} c_;

layout(location = 0) out struct {
    vec3 worldpos;
    vec3 lastworldpos;
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
    vec3 last_vertex_position = c_.last_positions_vbuffer.data[gl_VertexIndex];
    vec3 vertex_position = c_.positions_vbuffer.data[gl_VertexIndex];

    mat4x3 instance_transform = c_.instances_buffer.matrices[gl_InstanceIndex*2+0];
    mat4x3 last_instance_transform = c_.instances_buffer.matrices[gl_InstanceIndex*2+1];

    vec4 pos = c_.proj * float34_mul(c_.view, float34_mul(instance_transform, vertex_position).xyz);

    g_out.worldpos = float34_mul(instance_transform, vertex_position).xyz;
    g_out.lastworldpos = float34_mul(last_instance_transform, last_vertex_position).xyz;
    gl_Position = pos;
}