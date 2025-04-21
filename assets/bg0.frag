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
	d = sdBox(p, vec3(1.0));
	d = sdSphere(p, 1.0);
	return d;
}

vec3 unproject_dir(vec3 dir)
{
	vec3 result;
	result.x = c_.invproj[0][0] * dir.x;
	result.y = c_.invproj[1][1] * dir.y;
	result.z = -1;
	return result;
}

float project_depth(float depth)
{
	float z = c_.proj[2][2] * depth + c_.proj[2][3];
	float w = -depth;
	return z / w;
}

// Num of maximum marching steps for a ray
#define MAX_STEPS 100

// Max distance to surface, tells the ray that it completely missed
#define MAX_DISTANCE 800.0

// If ray is closer than this distance - then we have an intersection
#define SURFACE_DISTANCE 0.01

// Generate terrain heightfield with sinewaves. 0=2DNoise, 1=SineWaves
#define SINEWAVE_TERRAIN 1

// How many 2D sine waves should be added to terrain displacement
#define MAX_TERRAIN_WAVES 10

// Cut-off height for water. Used to override specular and base color of the waterplane
#define WATER_LINE_HEIGHT 1.0

// Display Capsules. 0=OFF, 1=ON
#define ENABLE_CAPSULES 0

// Display more SDF shapes. 0=OFF, 1=ON
#define ENABLE_SHAPES 0

// Cloud Toggle. 0=OFF, 1=ON
#define ENABLE_CLOUDS 1



// Global colors
const vec3 FOG_COLOR = 1.66 * vec3(0.1882, 0.2627, 0.3176);
const vec3 HORIZON_COLOR = 1.33 * vec3(0.43, 0.537, 0.592);
const vec3 ZENITH_COLOR = 1.15 *  vec3(0.33, 0.519, 0.63);
const vec3 SUN_COLOR = vec3(1.150, 1.00, 0.895);
const vec3 CLOUD_COLOR = vec3(1.0, 0.95, 1.0);

// ---------------------------------- NOISE FUNCTIONS ----------------------------------

// Calculate Fraction Brownian Motion noise from a White Noise Texture
// Used for cloud layer generation
// https://iquilezles.org/articles/morenoise/
float FractionBrownianMotion(in vec2 p)
{
    float f = 0.0;
    const mat2 m2 = mat2(0.8, -0.6, 0.6, 0.8);

#if 0
    f += 0.5000 * texture( iChannel0, p / 256.0 ).x; 
    p = m2 * p * 2.03;
    
    f += 0.2500 * texture( iChannel0, p / 256.0 ).x; 
    p = m2 * p * 2.03;
    
    f += 0.1250 * texture( iChannel0, p / 256.0 ).x; 
    p = m2 * p * 2.01;
    
    f += 0.0625 * texture( iChannel0, p / 256.0 ).x;
#endif
   
    return f / 0.9275;
}

// 2D Noise. Used to generate the terrain heightfield.
// https://www.shadertoy.com/view/tldSRj
vec2 g( vec2 n ) { return sin(n.x*n.y*vec2(12, 17)+vec2(1, 2)); }
float noise(vec2 p)
{
    const float kF = 2.0;
    vec2 i = floor(p);
	vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(sin(kF*dot(p,g(i+vec2(0,0)))),
               	   sin(kF*dot(p,g(i+vec2(1,0)))),f.x),
               mix(sin(kF*dot(p,g(i+vec2(0,1)))),
               	   sin(kF*dot(p,g(i+vec2(1,1)))),f.x),f.y);
}


// ---------------------------------- SIGNED DISTANCE FIELD FUNCTIONS ---------------------------------- 
float GetBoxDistance(vec3 pos, vec3 box)
{
  vec3 q = abs(pos) - box;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// SDF, Box with rounded edges
float GetRoundedBoxDistance(vec3 pos, vec3 box, float roundness)
{
  vec3 q = abs(pos) - box + roundness;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - roundness;
}

// SDF, Torus
float GetTorusDistance(vec3 position, vec2 radii)
{
  vec2 q = vec2(length(position.xz) - radii.x, position.y);
  return length(q) - radii.y;
}

// SDF, Vertical Capsule
float GetCapsuleDistance(vec3 position, float height, float radius)
{
  position.y -= clamp(position.y, 0.0, height);
  return length(position) - radius;
}

// Returns distance to terrain. 
// Heightfield is constructed in with two algorithms:
// - Adding multiple sine waves together
// - Generating a 2D noise with 4 octaves
float GetTerrainDistance(vec3 position)
{
#if SINEWAVE_TERRAIN == 1
    // These are sine wave params. Limit the amount of waves to defined MAX.
    int steps = clamp(min(10, MAX_TERRAIN_WAVES), 0, 10);
    float frequencies[10] = float[10](0.03, 0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0);
    float amplitudes[10] = float[10](111.0, 99.0, 88.973, 24.317, 8.797, 1.315, 1.0, 0.786, 0.531, 0.235);
    float offsets[10] = float[10](1.0, 0.973, 0.739, 0.573, 0.383, 0.179, 0.0, 0.354, 0.876, 0.222);
    
    // Iterate over arrays, add multiple sine waves
    float noise = 0.0;
    float noise_amplitude = 0.1;
    float noise_offset = 0.0;
    for(int i = 0; i <= steps - 1; i++)
    {
        noise += amplitudes[i] * sin(position.x * frequencies[i] + offsets[i]) * sin(position.z * frequencies[i] - offsets[i]); 
    }
    
    float terrain = noise * noise_amplitude - noise_offset;
#else
    // Calculate terrain Heightfield with 2D fractal noise, overlaying 4 octaves
    vec2 uv = position.xz * 0.01 + 0.0; 
    float terrain = 0.0;
    const mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );
    terrain  = 0.5000 * noise( uv ); 
    uv = m*uv;
    terrain += 0.2500 * noise( uv ); 
    uv = m*uv;
    terrain += 0.1250 * noise( uv ); 
    uv = m*uv;
    terrain += 0.0625 * noise( uv ); 
    uv = m*uv;
    terrain = -30.0 * terrain + 0.0;

#endif

    // Calculate radius around 0-coords. And convert it to [0-1] smooth mask
    float dist = length(position.xz);
    float dist_min = 175.0;
    float dist_max = 300.0;
    float alpha = clamp(dist - dist_min, 0.0, dist_max - dist_min) / 50.0;
    alpha = clamp(alpha, 0.0, 1.0);
    
    // Scale down the heightfield around the 0-coords, so terrain would be flat around the camera
    float mask = smoothstep(0.0, 1.0, alpha);
    float terrain_scale = mix(0.00, 1.0, mask);
    float out_distance = position.y + 1.0 + terrain * terrain_scale;
 
    return out_distance;
}

// Returs distance to the water plane at position
float GetWaterDistance(in vec3 position)
{
    float out_distance = position.y;
    
    // Calculate two sine waves
    float distance_a = 0.0055 * sin(c_.iTime * 1.657 + position.x) * sin(c_.iTime * 1.753 + position.z);
    float distance_b = 0.0495 * sin(c_.iTime * 1.01357 + position.x * 0.51) * sin(c_.iTime * 1.0353 + position.z * 0.51);
    
    // Blend waves based on distance to camera
    float depth = length(position.xz);
    float alpha = clamp(depth / 200.0, 0.0, 1.0);
    float water_distance = mix(distance_a, distance_b, alpha);
    
    out_distance += water_distance;
    
    return out_distance;
}

// Returns distance to closest surface at a position
float GetDistance(vec3 position)
{
    // Compare terrain and waterplane
    float terrain_distance = GetTerrainDistance(position);
    float water_distance = GetWaterDistance(position);
    float closest_distance =  min(terrain_distance, water_distance);
    
#if ENABLE_SHAPES >= 1
    // Sphere: position XYZ + radius W
    vec4 sphere_a = vec4(0.0, 2.0, 0.0, 1.0);
    float sphere_a_distance = length(position - sphere_a.xyz) - sphere_a.w;
    closest_distance = min(closest_distance, sphere_a_distance);
    
    // Box with rounded edges
    vec3 box_pos = vec3(-3.0, -2.0, 0.0) + position;
    vec3 box_size = vec3(0.8, 0.8, 0.8);
    float box_distance = GetRoundedBoxDistance(box_pos, box_size, 0.1);
    closest_distance = min(closest_distance, box_distance);
    
    // Torus
    vec3 torus_pos = vec3(3.0, -2.0, 0.0) + position;
    vec2 torus_radii = vec2(1.0, 0.333);
    float torus_distance = GetTorusDistance(torus_pos, torus_radii);
    closest_distance = min(closest_distance, torus_distance);

#endif
    
    
#if ENABLE_CAPSULES >= 1
    // Capsule Left
    vec3 capsule_left_pos = vec3(7.0, -1.25, 0.0) + position;
    float capsule_left_distance = GetCapsuleDistance(capsule_left_pos, 3.5, 1.0);
    closest_distance = min(closest_distance, capsule_left_distance);
    
    
    // Capsule Right
    vec3 capsule_right_pos = vec3(-7.0, -1.25, 0.0) + position;
    float capsule_right_distance = GetCapsuleDistance(capsule_right_pos, 3.5, 1.0);
    closest_distance = min(closest_distance, capsule_right_distance);
#endif
    
    return closest_distance;
}

// ---------------------------------- RENDERING FUNCTIONS ---------------------------------- 
// Returns a normal at the position
vec3 GetNormal(vec3 position)
{
    float dist = GetDistance(position);
    vec2 error = vec2(0.01, 0.0);
    
    vec3 normal = dist - vec3(
        GetDistance(position - error.xyy),
        GetDistance(position - error.yxy),
        GetDistance(position - error.yyx));
        
    return normalize(normal);
}

// Marches the ray until it hits a surface or goes way too far
float RayMarch(vec3 origin, vec3 direction)
{
    float distance_to_origin = 0.0;
    
    for(int i = 0; i < MAX_STEPS; i++)
    {
        vec3 position = origin + direction * distance_to_origin;
        float distance_to_surface = GetDistance(position);
        distance_to_origin += distance_to_surface;
        float error_threshold = SURFACE_DISTANCE * float(i);
        
        if(distance_to_origin > MAX_DISTANCE || distance_to_surface < error_threshold)
        {
            break; // stop marching
        }
    }
    
    return distance_to_origin;
}

// Returns direction to the sun light
vec3 GetLightDirection()
{
    vec3 position = vec3(0.0, 1.03, 5.0);
    position.y += sin(c_.iTime) * 0.05;
    
    return normalize(position);
}

// Returns direction to the light source (point)
float GetLight(vec3 position)
{
    vec3 light_direction = GetLightDirection();
    vec3 normal = GetNormal(position);
    
    // Diffuse lighting for a pixel
    float diffuse = clamp(dot(normal, light_direction), 0.0, 1.0); 
    
    // Cast ray from position in direction of the light
    vec3 shadow_sample_position = position + normal * SURFACE_DISTANCE * 2.0;
    float shadow = RayMarch(shadow_sample_position, light_direction);
    
    // Check if point is occluded from the light source
    if (shadow < length(light_direction * 10.0 - position))
    {
        diffuse *= 0.1;
    }
    
    return diffuse;
}

// Returns a shadow mask
float GetShadow(in vec3 position, in vec3 direction)
{
    vec3 normal = GetNormal(position);
    vec3 shadow_position = position + normal * SURFACE_DISTANCE * 2.0;
    
     // Check if point is occluded in the ray direction
    float shadow = RayMarch(shadow_position, direction);
    
    float shadow_mask = 1.0;
    if (shadow < length(direction * 10.0 - position))
    {
        shadow_mask *= 0.1;
    }
    
    return clamp(shadow_mask, 0.0, 1.0 );
}

// Applies exponential fog to the pixel
vec3 ApplyFog(in vec3 color, in float depth)
{
    float strength = 0.005;
    float amount = 1.0 - exp(-depth * strength);
    vec3 fog_color = FOG_COLOR;
    
    return mix(color, fog_color, amount);
}



// Returns alpha of the clouds
float GetCloudAlpha(in vec3 origin, in vec3 direction)
{
    // Scale for the noise
    vec2 scale = origin.xz + direction.xz * (35000.0 - origin.y) / direction.y; 
    
    // Animate offsets with time, so the clouds scroll across the sky
    vec2 offset_a = c_.iTime * 0.87 * vec2(1.0, 0.55);
    vec2 offset_b = c_.iTime * 0.63 * vec2(1.0, 0.33);
    
    // Calculate alphas for 2 cloud layers
    float cloud_a = 0.83 * smoothstep(0.45, 0.8, FractionBrownianMotion(scale / 4400.0 + offset_a)); 
    float cloud_b = 0.63 * smoothstep(0.17, 0.8, FractionBrownianMotion(scale / 1973.0 + offset_b));
    
    // Return the biggest alpha, so colors won't be blown out
    return max(cloud_a, cloud_b);
}

// Return sky color with clouds and sunglare
vec3 GetSkyColor(in vec3 origin, in vec3 direction)
{
    // Sky
    vec3 color = ZENITH_COLOR - direction.y * direction.y * 0.5;
    color = mix(color, HORIZON_COLOR, pow(1.0 - max(direction.y, 0.0), 3.0));

    // Sun: boost red channel of global sun color to get a yellow-ish color
    float sun_glare = clamp(dot(direction, GetLightDirection()), 0.0, 1.0);
    color += 0.225 * vec3(1.250, 1.0, 1.0) * SUN_COLOR * pow(sun_glare, 5.0);
    color += 0.175 * vec3(1.250, 1.0, 1.0) * SUN_COLOR * pow(sun_glare, 64.0);
    color += 0.190 * vec3(1.250, 1.0, 1.0) * SUN_COLOR * pow(sun_glare, 512.0);

    // Clouds: calculate alpha from the noise
#if ENABLE_CLOUDS == 1
    float alpha = GetCloudAlpha(origin, direction);
    color = mix(color, CLOUD_COLOR, alpha);
#endif

    // Horizon: blend fog color
    color = mix(color, FOG_COLOR, pow(1.0 - max(direction.y, 0.0), 16.0));
    
    return color;
}


// ---------------------------------- MAIN LOOP ---------------------------------- 
// Scene, consists of 4 elements represented by Signed Distance Fields: 
//    water plane, mountain terrain, skyscape and optional SDF shapes
// We raymarch through it, pixel by pixel.
// For each sky pixel we calculate cloud noise, sky color and sun glare
// For each non-sky pixel we calculate diffuse and specular lighting
void mainImage( out vec4 fragColor, out float out_depth, Ray ray )
{    
    // Camera position and direction
    vec3 ray_origin = ray.pos;
    vec3 ray_direction = ray.dir;

    // Raymarch the SDFs to get pixel depths
    // Get the attrbiutes that we'll need later for lighting
    float depth = RayMarch(ray_origin, ray_direction);
    vec3 position = ray_origin + ray_direction * depth;
    vec3 normal = GetNormal(position);
    vec3 reflection = reflect(ray_direction, normal);
    
    // Time to calculate the pixel color
    vec3 color = vec3(0.0);
    if( depth > MAX_DISTANCE)
    {
        // If ray didn't hit anything - it must be sky
        color = GetSkyColor(ray_origin, ray_direction);
        depth = -1.0;
	}
    else
    {
        // Material attributes
        vec3 base_color = vec3(0.4, 0.4, 0.4);
        float specular_strength = 0.5;
        
        // Override the color of capsules
        vec3 capsule_left_color = vec3(0.95, 0.4, 0.4);
        vec3 capsule_right_color = vec3(0.4, 0.4, 0.95);
        vec3 capsule_color = mix(capsule_left_color, capsule_right_color, step(position.x, 0.0));
        if(GetBoxDistance(position - vec3(0.0, 0.0, 0.0), vec3(10.0, 10.0, 4.0) ) < position.y - 1.0 )
        {
            // Make the capsules and other object SDFs red or blue
            base_color = 1.50 * mix(base_color, capsule_color, step(position.y, 5.0));
            specular_strength = 0.5;
        }

        // Override BaseColor and Specular strength for water plane
        if( position.y < WATER_LINE_HEIGHT )
        {
            base_color = vec3(0.13);
            specular_strength = 1.0;
        }
        
        // Lighting : Sun
        vec3 lit_color = vec3(0.0);
        {
            // Get diffuse light from the Sun
            float diffuse = GetLight(position); 
                  //diffuse *= GetShadow(position, GetLightDirection());

            // Get Specular from the Sun
            vec3 ligth_dir = GetLightDirection();
            vec3 hal = normalize(ligth_dir - ray_direction);
            float specular = pow( clamp( dot( normal, hal ), 0.0, 1.0 ),16.0);
                  specular *= diffuse;
                  specular *= 0.04 + 0.96 * pow( clamp( 1.0-dot(hal, ligth_dir), 0.0, 1.0), 5.0);

            // Accumulate lighting color (base color influenced by diffuse + specular)
            lit_color += base_color * 2.20 * diffuse * SUN_COLOR;
            lit_color += 5.00 * specular * SUN_COLOR * specular_strength;
        }

        // Lighting : Sky, diffuse + specular
        {
            vec3 sky_color = GetSkyColor(ray_origin, reflection);
           
            float diffuse = sqrt(clamp(0.5 + 0.5 * normal.y, 0.0, 1.0));
            float specular = smoothstep( -0.2, 0.2, reflection.y );
                  specular *= diffuse;
                  specular *= 0.04 + 0.96 * pow(clamp(1.0 + dot(normal, ray_direction), 0.0, 1.0), 5.0);
                  specular *= GetShadow(position, reflection);
                  
            // Add everything up together
            lit_color += 0.60 * base_color * diffuse * sky_color;
            lit_color += specular * sky_color * specular_strength;
        }
        
        color = lit_color;
    }
    
    
    // Add exponential fog on top of the image
    vec3 out_color = ApplyFog(color, depth);
    
    fragColor = vec4(out_color, 1.0);
    out_depth = depth;
}


vec4 ray_march(vec2 clip_space)
{
	vec3 camera_dir = unproject_dir(vec3(clip_space.x, clip_space.y, -1));
	Ray ray;
	ray.pos = c_.invview[3].xyz;
	ray.dir = mat3(c_.invview) * camera_dir.xyz;

	int step = 0;
	float h = 1.0;
	float t = 1.0;
	while (h > 0.01 && step < 512) {
	      h = sdScene(ray.pos + ray.dir * t);
	      t += h;
	      step++;
	}

	vec4 background = vec4(0.2, 0.1, 0.6, 1.0);
	return (h < 0.01) ? vec4(vec3(abs(h)), 1) : background;
}


void main()
{
	vec2 clip_space = g_in.uv * vec2(2.0) - vec2(1.0);
	vec3 camera_dir = unproject_dir(vec3(clip_space.x, clip_space.y, -1));
	Ray ray;
	ray.pos = c_.invview[3].xyz;
	ray.dir = mat3(c_.invview) * camera_dir.xyz;


	vec4 color;
	float depth;
	ray.pos.xy = ray.pos.xy * 10.0;
	mainImage(color, depth, ray);
	
	outColor = color;
	float projected_depth = project_depth(depth / 10.0) + 0.0000000001;
	gl_FragDepth = projected_depth;
}