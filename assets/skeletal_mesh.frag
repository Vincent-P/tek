#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec3 normal;
} g_in;

void main()
{
	vec3 normal = g_in.normal;

	vec3 L0 = vec3(1.0, 1.0, 1.0);
	float I0 = 1.0;
	float W0 = 1.0;
	vec3 Shading0 = vec3((dot(normal, normalize(L0)) + W0)/(1 + W0));
	
	vec3 L1 = vec3(-1.0, -1.0, -1.0);
	float I1 = 0.5;
	float W1 = 1.0;
	vec3 Shading1 = vec3((dot(normal, normalize(L1)) + W1)/(1 + W1));


	vec3 albedo = vec3(0.93);
	vec3 shading = albedo * (I0 * Shading0 + I1 * Shading1);
	
	outColor = vec4(shading, 1.0);
}