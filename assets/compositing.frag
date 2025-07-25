#version 450
#extension GL_EXT_scalar_block_layout : enable
#include "bindless.h"

#define IMGUI_TEXTURE_INPUT 0
#define HDR_TEXTURE_INPUT 1
#define LINEAR_TEXTURE_INPUT 2
#define IMAGE_OUTPUT 0

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec2 uv;
} g_in;

layout(scalar, push_constant) uniform uPushConstant {
    bool is_hdr;
} c_;

vec3 saturate(vec3 f)
{
	return clamp(f, vec3(0), vec3(1));
}

vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}


vec3 srgb_to_hdr10(vec3 color)
{
    //  Rec.709 -> Rec.2020
    const mat3 from709to2020 =
    {
        { 0.6274040f, 0.3292820f, 0.0433136f },
        { 0.0690970f, 0.9195400f, 0.0113612f },
        { 0.0163916f, 0.0880132f, 0.8955950f }
    };   
    color = from709to2020 * color;

    // ST.2084 (PQ curve)
    const float m1 = 2610.0 / 4096.0 / 4;
    const float m2 = 2523.0 / 4096.0 * 128;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 4096.0 * 32;
    const float c3 = 2392.0 / 4096.0 * 32;
    
    vec3 cp = pow(abs(color), vec3(m1));
    color   = pow((c1 + c2 * cp) / (1 + c3 * cp), vec3(m2));

    return color;
}

// Gamma ramps and encoding transfer functions
vec3 ApplySRGBCurve(vec3 linearCol)
{
    vec3 sRGBLo = linearCol * 12.92;
    vec3 sRGBHi = (pow(abs(linearCol), vec3(1.0/2.4)) * 1.055) - 0.055;
    bvec3 Mask = lessThan(linearCol, vec3(0.0031308));
    vec3 sRGB = mix(sRGBHi, sRGBLo, Mask);
    return sRGB;
}

// This is the new HDR transfer function, also called "PQ" for perceptual quantizer.  Note that REC2084
// does not also refer to a color space.  REC2084 is typically used with the REC2020 color space.
vec3 ApplyREC2084Curve(vec3 L)
{
    // Normalize HDR scene values ([0..>1] to [0..1]) for ST.2084 curve
    const float white_point = 350.0f; // 350 nits monitor
    const float st2084_max = 10000.0f;
    L *= white_point / st2084_max;

    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    vec3 Lp = pow(L, vec3(m1));
    return pow((c1 + c2 * Lp) / (1 + c3 * Lp), vec3(m2));
}

// Color space conversions, these assume linear (not gamma-encoded) values.
vec3 REC709toREC2020(vec3 RGB709)
{
    mat3 ConvMat;
    ConvMat[0] = vec3(0.627402, 0.069095, 0.016394);
    ConvMat[1] = vec3(0.329292, 0.919544, 0.088028);
    ConvMat[2] = vec3(0.043306, 0.011360, 0.895578);
    return ConvMat * RGB709;
}

const float m1 = 0.1593017578125;
const float m2 = 78.84375;
const float c1 = 0.8359375;
const float c2 = 18.8515625;
const float c3 = 18.6875;
float linear_to_pq(float L)
{
    float L_m1 = pow(L, m1);
    float numerator = c1 + c2 * L_m1;
    float denominator = 1.0 + c3 * L_m1;

    float N = pow(numerator / denominator, m2);
    return N;
}

vec3 linear_to_pq(vec3 L)
{
    vec3 N = vec3(linear_to_pq(L.r), linear_to_pq(L.g), linear_to_pq(L.b));
    return N;
}

void main()
{
    ivec2 pixel_coords = ivec2(g_in.uv * textureSize(global_textures_ms[HDR_TEXTURE_INPUT]));

    vec4 hdr0 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 0);
    vec4 hdr1 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 1);
    vec4 hdr2 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 2);
    vec4 hdr3 = texelFetch(global_textures_ms[HDR_TEXTURE_INPUT], pixel_coords, 3);
    vec4 hdr = (hdr0 + hdr1 + hdr2 + hdr3) * 0.25;
    
    vec4 srgb = texture(global_textures[LINEAR_TEXTURE_INPUT], g_in.uv);

    if (c_.is_hdr) {
    	
        vec3 mapped_hdr = hdr.rgb*vec3(350.0/10000.0);
	vec3 mapped_srgb = REC709toREC2020(srgb.rgb)*vec3(200.0/10000.0);
	float occlusion = 1.0 - srgb.a;

	vec3 composite = occlusion * mapped_hdr + mapped_srgb; 
	outColor = vec4(linear_to_pq(composite), 1.0);
	
    } else {
    
        vec3 mapped_hdr = ACESFilm(hdr.rgb);
	vec4 linear = srgb;
	float occlusion = 1.0 - linear.a;

	vec3 composite = occlusion * mapped_hdr + linear.rgb;
	outColor = vec4(ApplySRGBCurve(composite), 1.0);
	
    }
}