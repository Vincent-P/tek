#version 450
#extension GL_EXT_scalar_block_layout : enable

#include "hash.h"
#include "pbr.h"

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec3 normal;
    vec3 worldpos;
} g_in;

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4x3 view;
    mat4x3 invview;
    mat4x3 transform;
} c_;

void main()
{
	vec3 camera_pos = c_.invview[3].xyz;
	vec3 view = normalize(camera_pos - g_in.worldpos);
	vec3 normal = g_in.normal;
	vec3 r = reflect(-view, normal);

	brdf_params_t params;
	params.BaseColor = vec3(0.93);
	params.Metallic = 0.0;
	params.Roughness = 0.9;
	params.Reflectance = 0.5;
	params.Emissive = vec3(0.0);
	params.AmbientOcclusion = 1.0;

	vec3 shading = vec3(0.0);
#if 0
	#define SAMPLES 8
	for (int i = 0; i < SAMPLES; ++i)
	{
		uvec3 rng = pcg3d(uvec3(uvec2(gl_FragCoord), i));
		vec3 random_dir = hash_to_float3(rng);
		vec3 cosine_weighted_dir = normalize(normal + random_dir);
		
		shading += skyTex(cosine_weighted_dir) * BRDF(normal, view, cosine_weighted_dir, params);
	}
	shading = shading / float(SAMPLES);
#else
	shading += skyTex(normal) * BRDF(normal, view, normal, params);
	shading += (skyTex(r) * BRDF_Diffuse(normal, view, r, params));
#endif

	outColor = vec4(shading, 1.0);
}
