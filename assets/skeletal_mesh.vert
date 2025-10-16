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

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4x3 view;
    mat4x3 invview;
    mat4x3 transform;
    vec2 ibl_buffer;
    BoneMatricesBuffer bones_buffer;
    MeshVertexBuffer vbuffer;
} c_;

layout(location = 0) out struct {
    vec3 normal;
    vec3 worldpos;
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

vec4 float34_mul(mat4x3 m, vec3 v)
{
	vec4 result;
	result.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0];
	result.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1];
	result.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2];
	result.w = 1.0;
	return result;
}

mat3 adjugate(mat4x3 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz),
                cross(m[2].xyz, m[0].xyz),
                cross(m[0].xyz, m[1].xyz));

    /*
    // alternative way to write the adjoint

    return mat3(
     m[1].yzx*m[2].zxy-m[1].zxy*m[2].yzx,
     m[2].yzx*m[0].zxy-m[2].zxy*m[0].yzx,
     m[0].yzx*m[1].zxy-m[0].zxy*m[1].yzx );
    */

    /*
    // alternative way to write the adjoint

    return mat3(
     m[1][1]*m[2][2]-m[1][2]*m[2][1],
     m[1][2]*m[2][0]-m[1][0]*m[2][2],
     m[1][0]*m[2][1]-m[1][1]*m[2][0],
     m[0][2]*m[2][1]-m[0][1]*m[2][2],
	 m[0][0]*m[2][2]-m[0][2]*m[2][0],
     m[0][1]*m[2][0]-m[0][0]*m[2][1],
     m[0][1]*m[1][2]-m[0][2]*m[1][1],
     m[0][2]*m[1][0]-m[0][0]*m[1][2],
     m[0][0]*m[1][1]-m[0][1]*m[1][0] );
    */
}

void main()
{
    MeshVert vertex = c_.vbuffer.vertices[gl_VertexIndex];

    vec4 bone_weights = unpackUnorm4x8(vertex.bone_weights);
    uvec4 bone_indices = unpackUint4x8(vertex.bone_indices);

    mat4x3 bone_matrix = bone_weights[0] * c_.bones_buffer.matrices[bone_indices[0]]
    	   	       + bone_weights[1] * c_.bones_buffer.matrices[bone_indices[1]]
    	   	       + bone_weights[2] * c_.bones_buffer.matrices[bone_indices[2]]
    	   	       + bone_weights[3] * c_.bones_buffer.matrices[bone_indices[3]];

#if 0
    vec3 skinned_p0 = bone_weights[0] * (c_.bones_buffer.matrices[bone_indices[0]] * vec4(vertex.position, 1.0));
    vec3 skinned_p1 = bone_weights[1] * (c_.bones_buffer.matrices[bone_indices[1]] * vec4(vertex.position, 1.0));
    vec3 skinned_p2 = bone_weights[2] * (c_.bones_buffer.matrices[bone_indices[2]] * vec4(vertex.position, 1.0));
    vec3 skinned_p3 = bone_weights[3] * (c_.bones_buffer.matrices[bone_indices[3]] * vec4(vertex.position, 1.0));
    vec3 skinned_p = skinned_p0 + skinned_p1 + skinned_p2 + skinned_p3;
#else
    vec3 skinned_p = float34_mul(bone_matrix, vertex.position).xyz;
#endif

    vec4 pos = c_.proj * float34_mul(c_.view, float34_mul(c_.transform, skinned_p).xyz);

    g_out.normal = adjugate(c_.transform) * (adjugate(bone_matrix) * vertex.normal);
    g_out.worldpos = float34_mul(c_.transform, skinned_p).xyz;
    gl_Position = pos;
}