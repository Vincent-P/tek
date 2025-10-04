#pragma once

typedef struct Renderer Renderer;
typedef struct Camera Camera;
typedef struct RenderSkeletalMesh RenderSkeletalMesh;
struct SkeletalMeshInstance;
struct AssetLibrary;

struct Camera
{
	Float3 position;
	Float3 lookat;
	float vertical_fov;
};

/**

Renderer features:
- 3d meshes (same materials for ALL meshes, parametrized by material instances? or instance directly)
- lights? not really a feature but need to be registered somehow
- eventually particles
- debug draw
- imgui

life cycle of meshes / lights: create all resources at startup when loading assets, create instances ONCE during init, destroy all when shutdown

mesh instancing
instance data: transform + color

renderer_init_instances
renderer_register_skeletal_mesh_instance(skeletal mesh, &transform, &mesh pose data)
renderer_register_light_instance(light type, &transform, &light data)
renderer_init_instances_done

EITHER:
- register instance with immutable data (mesh/light reference, material data, transform)

game simulation
extract instance dynamic data (bone matrices, etc that was not cached in instance immutable data)
rendering
 **/

struct SkeletalMeshInstanceData
{
	// static
	struct SkeletalMeshAsset const *mesh;
	uint32_t mesh_render_handle;
	// dynamic
	struct SkeletalMeshInstance const *dynamic_data_mesh;
	struct SpatialComponent const *dynamic_data_spatial;
};

struct RendererTextureUpload
{
	void *temp_data;
	uint32_t texture;
	uint32_t x_offset;
	uint32_t y_offset;
	uint32_t width;
	uint32_t height;
};

// init
uint32_t renderer_get_size(void);
void renderer_init(Renderer *renderer, struct AssetLibrary *assets, SDL_Window *window);
void renderer_init_materials(Renderer *renderer, struct AssetLibrary *assets);
void renderer_create_render_skeletal_mesh(Renderer *renderer, struct SkeletalMeshAsset *asset, uint32_t handle);
// game init
void renderer_register_skeletal_mesh_instance(Renderer *renderer, struct SkeletalMeshInstanceData data);
void renderer_clear_skeletal_mesh_instances(Renderer *renderer);
// resources
void* renderer_temp_allocate_gpu(Renderer *renderer, uint32_t size);
void renderer_upload_texture(Renderer* renderer, struct RendererTextureUpload upload);
// runtime
void renderer_set_main_camera(Renderer *renderer, struct Camera camera);
void renderer_set_time(Renderer *renderer, float t);
void renderer_set_drawer2d(Renderer *renderer, struct Drawer2D *drawer);
void renderer_render(Renderer *renderer);
