#version 450
#include "hash.h"

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec3 normal;
    vec3 worldpos;
} g_in;

vec3 skyTex(vec3 ray)
{
	vec3 top = 2 * vec3(0.24,0.27,0.41);
	vec3 mid = vec3(0.07,0.11,0.21);
	float f = max(0.01, ray.z);
	return mix(mid, top, f);
}

void main()
{

	vec3 normal = g_in.normal;
	vec3 shading = vec3(0.0);
	#define SAMPLES 128
	for (int i = 0; i < SAMPLES; ++i)
	{
		uvec3 rng = pcg3d(uvec3(uvec2(gl_FragCoord), i));
		vec3 random_dir = hash_to_float3(rng);
		vec3 cosine_weighted_dir = normalize(normal + random_dir);
		shading += skyTex(cosine_weighted_dir);
	}
	vec3 albedo = vec3(0.93);
	shading = (albedo / float(SAMPLES)) * shading;
	
	
	outColor = vec4(shading, 1.0);
}
