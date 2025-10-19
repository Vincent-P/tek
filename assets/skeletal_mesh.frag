#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

#include "hash.h"
#include "pbr.h"
#include "sh.h"

layout(scalar, buffer_reference, buffer_reference_align=8) buffer IBLData
{
	SH_L2_RGB irradiance;
};

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
	IBLData ibl_buffer;
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
	params.Roughness = 0.2;
	params.Reflectance = 0.5;
	params.Emissive = vec3(0.0);
	params.AmbientOcclusion = 1.0;
	vec3 f0 = vec3(0.16 * params.Reflectance * params.Reflectance) * (1.0 - params.Metallic) + params.BaseColor *	params.Metallic;

	vec3 shading = vec3(0.0);
	// specular part
	vec3 AmbientLD = skyTex(r);
	shading += AmbientLD * DFG_Approximation(f0, params.Roughness, dot(normal, view));
	// diffuse part
	shading += GetSkyIrradiance(normal, c_.ibl_buffer.irradiance) * (params.BaseColor / M_PI);
	outColor = vec4(shading, 1.0);
}
