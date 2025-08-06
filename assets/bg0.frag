#version 450
#extension GL_EXT_scalar_block_layout : enable

#include "hash.h"
#include "pbr.h"

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec2 uv;
} g_in;

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4 invproj;
    mat4x3 view;
    mat4x3 invview;
    vec3 iResolution;
    float iTime;
} c_;

struct Ray
{
	vec3 pos;
	vec3 dir;
};

float sdSphere( vec3 p, float s )
{
	return length(p)-s;
}

float sdBox( vec3 p, vec3 b )
{
	vec3 q = abs(p) - b;
	return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdScene(vec3 p)
{
	float d = 0.0;
	// d = sdSphere(p, 1.0);
	// infinite plane (100x100)
	d = sdBox(p, vec3(50.0, 10.0, 0.01));
	return d;
}

vec3 unproject_dir(vec3 dir)
{
	vec4 result = (c_.invproj * vec4(dir, 1.0));
	result.xyz /= result.w;
	return result.xyz;
}

vec3 calcNormal( in vec3 p) // for function f(p)
{
    const float h = 0.0001; // replace by an appropriate value
    const vec2 k = vec2(1,-1);
    return normalize( k.xyy*sdScene( p + k.xyy*h ) + 
                      k.yyx*sdScene( p + k.yyx*h ) + 
                      k.yxy*sdScene( p + k.yxy*h ) + 
                      k.xxx*sdScene( p + k.xxx*h ) );
}

vec4 ray_march(uvec3 seed, vec2 clip_space, out float depth, out vec3 worldpos)
{
	vec3 camera_dir = unproject_dir(vec3(clip_space.x, clip_space.y, 1.0));
	Ray ray;
	ray.pos = c_.invview[3].xyz;
	ray.dir = normalize(mat3(c_.invview) * camera_dir.xyz);

	int step = 0;
	float h = 1.0;
	float t = 1.0;
	while (h > 0.01 && step < 512) {
	      h = sdScene(ray.pos + ray.dir * t);
	      t += h;
	      step++;
	}

	depth = t;
	
	vec4 background = vec4(skyTex(normalize(ray.dir)), 1.0);

	// shading
	vec3 p = ray.pos+ray.dir*t;
	vec3 normal = calcNormal(p);
	vec3 shading = vec3(0.0);
	vec3 view = normalize(-ray.dir);

	brdf_params_t params;
	params.BaseColor = vec3(0.33);
	params.Metallic = 0.5;
	params.Roughness = 0.99;
	params.Reflectance = 0.5;
	params.Emissive = vec3(0.0);
	params.AmbientOcclusion = 1.0;

	vec3 r = reflect(-view, normal);

	#define SAMPLES 8
	for (int i = 0; i < SAMPLES; ++i)
	{
		uvec3 rng = pcg3d(uvec3(seed.xy, i));
		vec3 random_dir = hash_to_float3(rng);
		vec3 cosine_weighted_dir = normalize(normal + random_dir);
		shading += skyTex(cosine_weighted_dir) * BRDF(normal, view, cosine_weighted_dir, params);
	}
	shading = shading / float(SAMPLES);
	shading += skyTex(r) * BRDF(normal, view, r, params);

	worldpos = p;
	return (h < 0.01) ? vec4(shading, 1) : background;
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

void main()
{
	uvec3 rng = uvec3(uvec2(gl_FragCoord), 1);
	float d = 0.0;
	vec2 clip_space = g_in.uv * vec2(2.0) - vec2(1.0);
	vec3 worldpos;
	outColor = ray_march(rng, clip_space, d, worldpos);

	vec4 projected = c_.proj * float34_mul(c_.view, worldpos);
	gl_FragDepth = projected.z / projected.w;
}
