#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

#include "asset.h"
#include "vulkan.h"
#include "renderer.h"

struct Application
{
	SDL_Window *window;
	Renderer *renderer;
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	struct Application *application = calloc(1, sizeof(struct Application));
	*appstate = application;

	SDL_Init(SDL_INIT_VIDEO);

	application->window = SDL_CreateWindow("tek", 1280, 1280, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

	application->renderer = calloc(1, renderer_get_size());
	renderer_init(application->renderer, application->window);
	
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	struct Application *application = appstate;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	struct Application *application = appstate;
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}
	
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	struct Application *application = appstate;

	renderer_render(application->renderer);

	return SDL_APP_CONTINUE;
}

#include "vulkan.c"
#include "renderer.c"
