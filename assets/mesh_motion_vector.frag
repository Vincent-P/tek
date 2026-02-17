#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

#include "hash.h"
#include "pbr.h"
#include "sh.h"

vec4 float34_mul(mat4x3 m, vec3 v)
{
	vec4 result;
	result.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0];
	result.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1];
	result.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2];
	result.w = 1.0;
	return result;
}

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4x3 view;
    mat4x3 invview;
} c_;

layout(location = 0) in struct {
	vec3 worldpos;
	vec3 lastworldpos;
} g_in;

layout(location = 0) out vec4 outMotionVector;
void main()
{
	vec4 current_clip = c_.proj * float34_mul(c_.view, g_in.worldpos.xyz);
	vec4 last_clip = c_.proj * float34_mul(c_.view, g_in.lastworldpos.xyz);
	vec2 velocity = (current_clip.xy / current_clip.w) - (last_clip.xy / last_clip.w);
	outMotionVector = vec4(velocity * 0.5, 0, 1); // uv [0;1], clip is [-1;1]
}
