#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <dcimgui.h>

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

#include "asset.h"
#include "vulkan.h"
#include "renderer.h"
#include "inputs.h"
#include "game.h"
#include "debugdraw.h"
#include "file.h"

struct Game
{
	struct GameState gs;
	struct NonGameState ngs;
};

struct Application
{
	SDL_Window *window;
	struct Inputs inputs;
	struct AssetLibrary assets;
	struct Game game;
	Renderer *renderer;

	uint64_t current_time;
	uint64_t accumulator;
	uint64_t t;
};

static void load_assets(struct AssetLibrary *assets, struct Renderer *renderer)
{
	Serializer s = {0};

	s = serialize_begin_read_file("cooking/1812931262");
	SkeletalMeshWithAnimationsAsset skeletal_mesh_with_animations;
	Serialize_SkeletalMeshWithAnimationsAsset(&s, &skeletal_mesh_with_animations);
	serialize_end_read_file(&s);

	AssetId anim_skeleton_id = asset_library_add_anim_skeleton(assets, skeletal_mesh_with_animations.anim_skeleton);
	asset_library_add_skeletal_mesh(assets, skeletal_mesh_with_animations.skeletal_mesh);
	for (uint32_t ianim = 0; ianim < skeletal_mesh_with_animations.animations_length; ++ianim) {
		asset_library_add_animation(assets, skeletal_mesh_with_animations.animations[ianim]);
	}

	const char* materials[] = {
		"cooking/3661877039",
		"cooking/3200094153",
		"cooking/2455425701",
		"cooking/295627051",
	};
	for (uint32_t i = 0; i < ARRAY_LENGTH(materials); ++i) {
		struct MaterialAsset material = {0};
		s = serialize_begin_read_file(materials[i]);
		Serialize_MaterialAsset(&s, &material);
		serialize_end_read_file(&s);
		asset_library_add_material(assets, material);
	}
}

static void postload_assets(struct AssetLibrary *assets, struct Renderer *renderer)
{
	for (uint32_t iskeletal_mesh = 0; iskeletal_mesh < ASSET_SKELETAL_MESH_CAPACITY; ++iskeletal_mesh) {
		if (assets->skeletal_meshes_id[iskeletal_mesh] == 0) {
			break;
		}

		SkeletalMeshAsset *skeletal_mesh = assets->skeletal_meshes + iskeletal_mesh;
		renderer_create_render_skeletal_mesh(renderer, skeletal_mesh, iskeletal_mesh);
	}
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	struct Application *application = calloc(1, sizeof(struct Application));
	*appstate = application;

	// window init
	SDL_Init(SDL_INIT_VIDEO);
	application->window = SDL_CreateWindow("tek", 1280, 1280, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

	// load all assets
	load_assets(&application->assets, application->renderer);
	
	// renderer and imgui init
	ImGui_CreateContext(NULL);
	application->renderer = calloc(1, renderer_get_size());
	renderer_init(application->renderer, &application->assets, application->window);

	postload_assets(&application->assets, application->renderer);

	// game init
	application->game.ngs.assets = &application->assets;
	game_state_init(&application->game.gs, &application->game.ngs, application->renderer);
	
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
	inputs_process_event(event, &application->inputs);
	
	return SDL_APP_CONTINUE;
}

static void game_run_frame(struct Application *application)
{
	struct GameInputs inputs = game_read_input(&application->inputs);
	// ggpo_add_local_input

	// ggpo_synchronize_input
	game_simulate_frame(&application->game.ngs, &application->game.gs, inputs);

	application->game.ngs.t += 0.1f;
	application->game.ngs.frame_number += 1;
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

	// accumulate time
	uint64_t new_time = SDL_GetTicks();
	uint64_t frame_time = new_time - application->current_time;
	application->current_time = new_time;
	application->accumulator += frame_time;
	if (ImGui_Begin("Application", NULL, 0)) {
		ImGui_Text("current time: %llu", application->current_time);
		ImGui_Text("time: %llu", application->t);
		ImGui_Text("frame_time: %llu", frame_time);
		ImGui_Text("accumulator: %llu", application->accumulator);
	}
	ImGui_End();

	// simulate in fixed-step increments from the accumulator
	const uint64_t dt = 33;
	while (application->accumulator >= dt) {
		game_run_frame(application);
		application->t += dt;
		application->accumulator -= dt;
	}
	
	game_render(&application->game.ngs, &application->game.gs);
	renderer_set_time(application->renderer, ((float)application->current_time) / 1000.0f);
	renderer_render(application->renderer, &application->game.ngs.camera);

	return SDL_APP_CONTINUE;
}

#include "asset.c"
#include "vulkan.c"
#include "debugdraw.c"
#include "renderer.c"
#include "game.c"
#include "inputs.c"
#include "anim.c"
#include <ufbx.c>

#include "editor.c"
