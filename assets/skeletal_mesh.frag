#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in struct {
    vec3 normal;
} g_in;

void main()
{
	vec3 normal = g_in.normal;

	vec3 L0 = vec3(-1.0, 0.2, -1.0);
	float I0 = 0.75;
	float W0 = 1.0;
	float Shading0 = (dot(normal, normalize(L0)) + W0)/(1 + W0);
	
	vec3 L1 = vec3(1.0, -1.0, 0.0);
	float I1 = 0.75;
	float W1 = 1.0;
	float Shading1 = (dot(normal, normalize(L1)) + W1)/(1 + W1);

	vec3 albedo = normal * vec3(0.5) + vec3(0.5);
	
	vec3 shading = albedo * (I0 * Shading0 + I1 * Shading1);
	vec4 color = vec4(albedo * shading, 1.0);
	outColor = color;
}