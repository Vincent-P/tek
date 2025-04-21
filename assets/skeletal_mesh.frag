#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec3 normal;
} g_in;

void main()
{
	vec3 normal = g_in.normal;

	vec3 L0 = vec3(0.0, 0.0, -1.0);
	float I0 = 0.7;
	float W0 = 1.2;
	vec3 Shading0 = vec3((dot(normal, normalize(L0)) + W0)/(1 + W0));
	
	vec3 L1 = vec3(1.0, 1.0, 1.0);
	float I1 = 0.5;
	float W1 = 1.0;
	vec3 Shading1 = ((dot(normal, normalize(L1)) + W1)/(1 + W1)) * vec3(0.0, 0.2, 0.8);
	
	vec3 L3 = vec3(1.0, 0.0, 1.0);
	float I3 = 0.2;
	float W3 = 0.5;
	vec3 Shading3 = vec3((dot(normal, normalize(L3)) + W3)/(1 + W3));

	vec3 albedo = vec3(0.93);
	albedo = vec3(0.5, 0.21, 0.05);
	
	vec3 shading = albedo * (I0 * Shading0 + I1 * Shading1) + I3 * Shading3;
	vec4 color = vec4(shading, 1.0);
	outColor = color;
}