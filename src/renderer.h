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

uint32_t renderer_get_size(void);
void renderer_init(Renderer *renderer, struct AssetLibrary *assets, SDL_Window *window);
void renderer_render(Renderer *renderer, struct Camera *camera);

void renderer_set_time(Renderer *renderer, float t);
void renderer_create_render_skeletal_mesh(Renderer *renderer, struct SkeletalMeshAsset *asset, uint32_t handle);
void renderer_submit_skeletal_instance(Renderer *renderer, struct SkeletalMeshInstance* instance);
