#define PI 3.14

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

// Lights

vec3 skyTex(vec3 ray)
{
	vec3 top = 3.0 * vec3(0.1,0.27,0.61);
	vec3 mid = 1.0 * vec3(0.2, 0.3, 0.7);
	vec3 bottom = 3.0 * vec3(0.05,0.05,0.1);
	vec3 target = (ray.z < 0.0) ? bottom : top;
	return mix(mid, target, abs(ray.z));
}

