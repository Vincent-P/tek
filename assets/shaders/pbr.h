#include "sh.h"

#define PI 3.14
float saturate(float x) { return clamp(x, 0.0, 1.0); }

// Specular BRDF
// Cook-Torrance specular microfacet model, with a GGX normal distribution function, a Smith-GGX height-correlated visibility function, and a Schlick Fresnel function

// Normal distribution function (specular D)
float D_GGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

// Geometric shadowing (specular G)
// Heitz formulation replace G with V
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a2 = roughness * roughness;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

// Fresnel (specular F)
vec3 F_Schlick(float u, vec3 f0, float f90)
{
    return f0 + (vec3(f90) - f0) * pow(1.0 - u, 5.0);
}

// Diffuse BRDF
// Lambertian diffuse model

float Fd_Lambert()
{
    return 1.0 / PI;
}

// https://knarkowicz.wordpress.com/2014/12/27/analytical-dfg-term-for-ibl/
// GGX distribution, Smith geometry and Schlick’s Fresnel approximation
vec3 DFG_Approximation(vec3 specularColor, float roughness, float NoV)
{
	// gloss = (1 - roughness)^4
	float x = pow((1 - roughness), 4);
	float y = NoV;

	float b1 = -0.1688;
	float b2 = 1.895;
	float b3 = 0.9903;
	float b4 = -4.853;
	float b5 = 8.404;
	float b6 = -5.069;
	float bias = saturate( min( b1 * x + b2 * x * x, b3 + b4 * y + b5 * y * y + b6 * y * y * y ) );

	float d0 = 0.6045;
	float d1 = 1.699;
	float d2 = -0.5228;
	float d3 = -3.603;
	float d4 = 1.404;
	float d5 = 0.1939;
	float d6 = 2.661;
	float delta = saturate( d0 + d1 * x + d2 * y + d3 * x * x + d4 * x * y + d5 * y * y + d6 * x * x * x );
	float scale = delta - bias;

	bias *= saturate( 50.0 * specularColor.y );
	return specularColor * scale + bias;
}

// Final BRDF
struct brdf_params_t
{
	vec3 BaseColor;  // non-metal: albedo | metal: specular color
	float Metallic;  // dialetric 0 | conductor 1
	float Roughness; // 0=smooth | 1=rough
	float Reflectance; // 0.35=water | 0.5=common | 0.7=ruby | 1.0=diamond
	vec3 Emissive;   // emission
	float AmbientOcclusion; //
};

vec3 BRDF_Specular(vec3 n, vec3 v, vec3 l, brdf_params_t params)
{
    vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    // Parametrization
    // perceptually linear roughness to roughness
    float roughness = params.Roughness * params.Roughness;
    // dialetric have achromatic specular reflectance but conductors use base color as specular color
    vec3 diffuseColor = (1.0 - params.Metallic) * params.BaseColor;
    // non-metal: achromatic reflectance | metals: base color
    vec3 f0 = vec3(0.16 * params.Reflectance * params.Reflectance) * (1.0 - params.Metallic) + params.BaseColor * params.Metallic;
    float f90 = 1.0;

    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0, f90);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    // specular BRDF
    vec3 Fr = (D * V) * F;
    return Fr;
}

vec3 BRDF_Diffuse(vec3 n, vec3 v, vec3 l, brdf_params_t params)
{
    vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    // Parametrization
    // perceptually linear roughness to roughness
    float roughness = params.Roughness * params.Roughness;
    // dialetric have achromatic specular reflectance but conductors use base color as specular color
    vec3 diffuseColor = (1.0 - params.Metallic) * params.BaseColor;
    // non-metal: achromatic reflectance | metals: base color
    vec3 f0 = vec3(0.16 * params.Reflectance * params.Reflectance) * (1.0 - params.Metallic) + params.BaseColor * params.Metallic;
    float f90 = 1;

    // diffuse BRDF
    vec3 Fd = diffuseColor * Fd_Lambert();
    return Fd;
}

vec3 BRDF(vec3 n, vec3 v, vec3 l, brdf_params_t params)
{
    vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    // Parametrization
    // perceptually linear roughness to roughness
    float roughness = params.Roughness * params.Roughness;
    // dialetric have achromatic specular reflectance but conductors use base color as specular color
    vec3 diffuseColor = (1.0 - params.Metallic) * params.BaseColor;
    // non-metal: achromatic reflectance | metals: base color
    vec3 f0 = vec3(0.16 * params.Reflectance * params.Reflectance) * (1.0 - params.Metallic) + params.BaseColor * params.Metallic;
    float f90 = 1;

    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0, f90);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    // specular BRDF
    vec3 Fr = (D * V) * F;

    // diffuse BRDF
    vec3 Fd = diffuseColor * Fd_Lambert();

    vec3 result = Fr + Fd;

    // result = Fd;

    return result;
}

// -- Lights
// Computes the camera's EV100 from exposure settings
// aperture in f-stops
// shutterSpeed in seconds
// sensitivity in ISO
float exposureSettings(float aperture, float shutterSpeed, float sensitivity) {
    return log2((aperture * aperture) / shutterSpeed * 100.0 / sensitivity);
}

// Computes the exposure normalization factor from
// the camera's EV100
float exposure(float ev100) {
    return 1.0 / (pow(2.0, ev100) * 1.2);
}

float PreExposeLight(highp float intensity)
{
    // sunny 16
    highp float ev100 = exposureSettings(16, 1.0/100.0, 100.0);
    highp float exposure = exposure(ev100);
    return intensity * exposure;
}

vec3 skyTex(vec3 ray)
{
	vec3 top    = PreExposeLight(120000.0) * vec3(0.20, 0.27, 0.91);
	vec3 mid    = PreExposeLight( 20000.0) * vec3(0.20, 0.30, 0.70);
	vec3 bottom = PreExposeLight( 20000.0) * vec3(0.33, 0.33, 0.93);

	vec3 target = (ray.z < 0.0) ? bottom : top;
	return mix(mid, target, abs(ray.z));
}

vec3 GetSkyIrradiance(vec3 normal, SH_L2_RGB AmbientIrradianceSH)
{
#if 0
	const int SAMPLES = 128;
	vec3 irradiance = vec3(0.0);
	for (int i = 0; i < SAMPLES; ++i) {
		uvec3 rng = pcg3d(uvec3(i, SAMPLES, 0));
		vec3 random_dir = hash_to_float3(rng);
		vec3 cosine_weighted_dir = normalize(normal + random_dir);
		irradiance += skyTex(cosine_weighted_dir);
	}
	return irradiance / float(SAMPLES);
#else
	return SH_CalculateIrradiance(AmbientIrradianceSH, normal);
#endif
}
