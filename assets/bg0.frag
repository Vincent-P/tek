#version 450
#extension GL_EXT_scalar_block_layout : enable

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
	d = sdBox(p, vec3(50.0, 50.0, 0.01));
	return d;
}

vec3 unproject_dir(vec3 dir)
{
#if 0
	vec3 result;
	result.x = c_.invproj[0][0] * dir.x;
	result.y = 1;
	result.z = c_.invproj[2][1] * dir.y;
	return result;
#else
	vec4 result = (c_.invproj * vec4(dir, 1.0));
	result.xyz /= result.w;
	return result.xyz;
#endif
}

float project_depth(float depth)
{
	float z = c_.proj[2][1] * depth + c_.proj[2][3];
	float w = depth;
	return z / w;
}

vec4 ray_march(vec2 clip_space, out float depth)
{
	vec3 camera_dir = unproject_dir(vec3(clip_space.x, clip_space.y, 1.0));
	Ray ray;
	ray.pos = c_.invview[3].xyz;
	ray.dir = normalize(mat3(c_.invview) * camera_dir.xyz);

	vec4 background = vec4(ray.dir, 1.0);

	int step = 0;
	float h = 1.0;
	float t = 1.0;
	while (h > 0.01 && step < 512) {
	      h = sdScene(ray.pos + ray.dir * t);
	      t += h;
	      step++;
	}

	depth = t;
	
	return (h < 0.01) ? vec4(vec3(abs(h)), 1) : background;
}


void main()
{
	float d = 0.0;
	vec2 clip_space = g_in.uv * vec2(2.0) - vec2(1.0);
	
	outColor = ray_march(clip_space, d);
	gl_FragDepth = project_depth(d);
}