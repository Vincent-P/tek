#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <assert.h>
#include <dcimgui.h>

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

	ImGui_CreateContext(NULL);
	
	application->renderer = calloc(1, renderer_get_size());
	renderer_init(application->renderer, application->window);
	
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	struct Application *application = appstate;
}

bool imgui_process_event(struct Application *application, SDL_Event* event)
{
	ImGuiIO* io = ImGui_GetIO();

	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
		{
			float mouse_pos_x = (float)event->motion.x;
			float mouse_pos_y = (float)event->motion.y;
			ImGuiIO_AddMouseSourceEvent(io, event->motion.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
			ImGuiIO_AddMousePosEvent(io, mouse_pos_x, mouse_pos_y);
			return true;
		}
	case SDL_EVENT_MOUSE_WHEEL:
		{
			//IMGUI_DEBUG_LOG("wheel %.2f %.2f, precise %.2f %.2f\n", (float)event->wheel.x, (float)event->wheel.y, event->wheel.preciseX, event->wheel.preciseY);
			float wheel_x = -event->wheel.x;
			float wheel_y = event->wheel.y;
			ImGuiIO_AddMouseSourceEvent(io, event->wheel.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
			ImGuiIO_AddMouseWheelEvent(io, wheel_x, wheel_y);
			return true;
		}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			int mouse_button = -1;
			if (event->button.button == SDL_BUTTON_LEFT) { mouse_button = 0; }
			if (event->button.button == SDL_BUTTON_RIGHT) { mouse_button = 1; }
			if (event->button.button == SDL_BUTTON_MIDDLE) { mouse_button = 2; }
			if (event->button.button == SDL_BUTTON_X1) { mouse_button = 3; }
			if (event->button.button == SDL_BUTTON_X2) { mouse_button = 4; }
			if (mouse_button == -1)
				break;
			ImGuiIO_AddMouseSourceEvent(io, event->button.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
			ImGuiIO_AddMouseButtonEvent(io, mouse_button, (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN));
			return true;
		}
	case SDL_EVENT_TEXT_INPUT:
		{
			ImGuiIO_AddInputCharactersUTF8(io, event->text.text);
			return true;
		}
	}
	return false;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
	struct Application *application = appstate;
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}

	imgui_process_event(application, event);
	
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	struct Application *application = appstate;

	// Setup display size (every frame to accommodate for window resizing)
	ImGuiIO *io = ImGui_GetIO();
	int w, h;
	int display_w, display_h;
	SDL_GetWindowSize(application->window, &w, &h);
	if (SDL_GetWindowFlags(application->window) & SDL_WINDOW_MINIMIZED)
		w = h = 0;
	SDL_GetWindowSizeInPixels(application->window, &display_w, &display_h);
	io->DisplaySize.x = (float)w;
	io->DisplaySize.y = (float)h;
	if (w > 0 && h > 0) {
		io->DisplayFramebufferScale.x = (float)display_w / w;
		io->DisplayFramebufferScale.y = (float)display_h / h;
	}
	ImGui_NewFrame();

	bool opened = true;
	ImGui_ShowDemoWindow(&opened);

	renderer_render(application->renderer);

	return SDL_APP_CONTINUE;
}

#include "asset.c"
#include "vulkan.c"
#include "renderer.c"
