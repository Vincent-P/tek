#pragma once

typedef struct Renderer Renderer;
typedef struct Camera Camera;

struct Camera
{
	Float3 position;
	Float3 lookat;
	float vertical_fov;
};

uint32_t renderer_get_size(void);
void renderer_init(Renderer *renderer, SDL_Window *window);
void renderer_render(Renderer *renderer, struct Camera *camera);
