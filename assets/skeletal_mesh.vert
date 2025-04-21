#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

struct MeshVert
{
	vec3 position;
	uint bone_indices;
	vec3 normal;
	uint bone_weights;
};

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer MeshVertexBuffer
{
	MeshVert vertices[];
};

layout(scalar, buffer_reference, buffer_reference_align=8) readonly buffer BoneMatricesBuffer
{
	mat4x3 matrices[];
};

layout(push_constant) uniform uPushConstant {
    mat4 proj;
    mat4 view;
    BoneMatricesBuffer bones_buffer;
    MeshVertexBuffer vbuffer;
} c_;

layout(location = 0) out struct {
    vec3 normal;
} g_out;

uvec4 unpackUint4x8(uint v)
{
	uvec4 result; 
	result.a =  (v >> 24) & 0xff;
	result.b =  (v >> 16) & 0xff;
	result.g =  (v >> 8) & 0xff;
	result.r =  (v >> 0) & 0xff;
	return result;
}

void main() 
{
    MeshVert vertex = c_.vbuffer.vertices[gl_VertexIndex];
    
    vec4 bone_weights = unpackUnorm4x8(vertex.bone_weights);
    uvec4 bone_indices = unpackUint4x8(vertex.bone_indices);
    vec3 skinned_p0 = bone_weights[0] * (c_.bones_buffer.matrices[bone_indices[0]] * vec4(vertex.position, 1.0));
    vec3 skinned_p1 = bone_weights[1] * (c_.bones_buffer.matrices[bone_indices[1]] * vec4(vertex.position, 1.0));
    vec3 skinned_p2 = bone_weights[2] * (c_.bones_buffer.matrices[bone_indices[2]] * vec4(vertex.position, 1.0));
    vec3 skinned_p3 = bone_weights[3] * (c_.bones_buffer.matrices[bone_indices[3]] * vec4(vertex.position, 1.0));
    vec3 skinned_p = skinned_p0 + skinned_p1 + skinned_p2 + skinned_p3;

    vec4 pos = (c_.proj * (c_.view * vec4(skinned_p, 1.0)));

    g_out.normal = vertex.normal;
    gl_Position = pos;
}