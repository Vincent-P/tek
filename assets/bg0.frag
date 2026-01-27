#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

#include "hash.h"
#include "pbr.h"
#include "sh.h"

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec2 uv;
} g_in;

layout(scalar, buffer_reference, buffer_reference_align=8) buffer IBLData
{
	SH_L2_RGB irradiance;
};

layout(scalar, push_constant) uniform uPushConstant {
    mat4 proj;
    mat4 invproj;
    mat4x3 view;
    mat4x3 invview;
    vec3 iResolution;
    float iTime;
    IBLData ibl_buffer;
} c_;

struct Ray
{
	vec3 pos;
	vec3 dir;
};

float sdBox( vec3 p, vec3 b )
{
	vec3 q = abs(p) - b;
	return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}


float sdRoundBox( vec3 p, vec3 b, float r )
{
  vec3 q = abs(p) - b + r;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r;
}

float opOnion( in float sdf, in float thickness )
{
    return abs(sdf)-thickness;
}

vec3 opRepetition( in vec3 p, in vec3 s )
{
    return p - s*round(p/s);
}

float sdWall(vec3 p)
{
	p = p - vec3(0, 0, 0.66);
	float d = sdRoundBox(p, vec3(0.5, 0.5, 0.5), 0.066);

	p = p - vec3(0.37, -1.39, 0.16);
	d = min(d, sdRoundBox(p, vec3(0.66, 0.66, 0.66), 0.033));

	return d;
}

float sdWalls(vec3 p)
{
    // domain repetition
    const int   n = 48;
    const float r = 24.0;

    const float b = 6.283185/float(n);
    float a = atan(p.y,p.x);
    float i = floor(a/b);

    float c1 = b*(i+0.0); vec2 _p1 = mat2(cos(c1),-sin(c1),sin(c1), cos(c1))*p.xy;
    float c2 = b*(i+1.0); vec2 _p2 = mat2(cos(c2),-sin(c2),sin(c2), cos(c2))*p.xy;

    vec3 p1 = vec3(_p1.x - r, _p1.y, p.z);
    vec3 p2 = vec3(_p2.x - r, _p2.y, p.z);

    // evaluate two SDF instances
#define sdf(_p,_id) sdWall(_p)
    return min(sdf(p1,id+0), sdf(p2,id+1));
#undef sdf
}

float sdScene(vec3 p)
{
	p.z += 0.02;
	return sdBox(p, vec3(100.0,100.0,0.001));

	p = p - vec3(0,0,-0.04);

	// repeated floor tiles
	vec3 step = vec3(0.5, 0.5, 0.0);

	vec3 id = round(p/step);
	vec3 rep_p = p - step*id;
	vec3 mir_p = vec3(((int(id.x)&1)==0) ? rep_p.x : -rep_p.x,
	((int(id.y)&1)==0) ? rep_p.y : -rep_p.y,
	((int(id.z)&1)==0) ? rep_p.z : -rep_p.z);
	float d = sdRoundBox(mir_p, vec3(0.24, 0.24, 0.01), 0.01);

	d = min(d, sdWalls(p));

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

	vec4 background = vec4(skyTex(normalize(ray.dir)) / M_PI, 1.0);

	// shading
	brdf_params_t params;
	params.BaseColor = vec3(0.5);
	params.Metallic = 0.0;
	params.Roughness = 0.5;
	params.Reflectance = 0.5;
	params.Emissive = vec3(0.0);
	params.AmbientOcclusion = 1.0;
	vec3 f0 = vec3(0.16 * params.Reflectance * params.Reflectance) * (1.0 - params.Metallic) + params.BaseColor *	params.Metallic;

	vec3 p = ray.pos+ray.dir*t;
	vec3 normal = calcNormal(p);
	vec3 view = normalize(-ray.dir);
	vec3 r = reflect(-view, normal);

	vec3 shading = vec3(0.0);
	// specular part
	vec3 AmbientLD = skyTex(r);
	shading += AmbientLD * DFG_Approximation(f0, params.Roughness, dot(normal, view));
	// diffuse part
	shading += GetSkyIrradiance(normal, c_.ibl_buffer.irradiance) * (params.BaseColor / M_PI);

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
