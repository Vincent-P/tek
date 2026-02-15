#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_timer.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <dcimgui.h>
#include <tracy/tracy/TracyC.h>
#include <clay.h>

#include "core.h"
#include "asset.h"
#include "vulkan.h"
#include "renderer.h"
#include "inputs.h"
#include "tek.h"
#include "game.h"
#include "game_battle.h"
#include "debugdraw.h"
#include "file.h"
#include "watcher.h"
#include "clay_integration.h"
#include "drawer2d.h"
#include "ui.h"

#include <windows.h>

struct Application
{
	SDL_Window *window;
	struct Inputs inputs;
	struct AssetLibrary assets;
	struct Game game;
	struct Drawer2D *drawer;
	Renderer *renderer;

	uint64_t current_time;
	uint64_t f;
};


static void load_assets_materials(struct AssetLibrary *assets, struct Renderer *renderer)
{
	Serializer s = {0};

	const char* materials[] = {
		"cooking/3661877039",
		"cooking/3200094153",
		"cooking/2455425701",
		"cooking/295627051",
		"cooking/4245599685",
		"cooking/3339345721"
	};
	for (uint32_t i = 0; i < ARRAY_LENGTH(materials); ++i) {
		struct MaterialAsset material = {0};
		s = serialize_begin_read_file(materials[i]);
		Serialize_MaterialAsset(&s, &material);
		serialize_end_read_file(&s);
		asset_library_add_material(assets, material);
	}

	const char* programs[] = {
		"cooking/3356797155",
		"cooking/4212483871",
		"cooking/4060554051",
	};
	for (uint32_t i = 0; i < ARRAY_LENGTH(programs); ++i) {
		struct ComputeProgramAsset program = {0};
		s = serialize_begin_read_file(programs[i]);
		Serialize_ComputeProgramAsset(&s, &program);
		serialize_end_read_file(&s);
		asset_library_add_compute_program(assets, program);
	}
}

static void load_assets(struct AssetLibrary *assets, struct Renderer *renderer)
{
	Serializer s = {0};

	s = serialize_begin_read_file("cooking/3227071964");
	SkeletalMeshWithAnimationsAsset skeletal_mesh_with_animations;
	Serialize_SkeletalMeshWithAnimationsAsset(&s, &skeletal_mesh_with_animations);
	serialize_end_read_file(&s);

	AssetId anim_skeleton_id = asset_library_add_anim_skeleton(assets, skeletal_mesh_with_animations.anim_skeleton);
	asset_library_add_skeletal_mesh(assets, skeletal_mesh_with_animations.skeletal_mesh);
	for (uint32_t ianim = 0; ianim < skeletal_mesh_with_animations.animations_length; ++ianim) {
		asset_library_add_animation(assets, skeletal_mesh_with_animations.animations[ianim]);
	}

	load_assets_materials(assets, renderer);

	// tek
	tek_read_character_json();
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

static void ImGui_ImplSDL3_PlatformSetImeData(ImGuiContext*, ImGuiViewport* viewport, ImGuiPlatformImeData* data);
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	TracyCZoneN(f, "AppInit", true);

	struct Application *application = calloc(1, sizeof(struct Application));
	*appstate = application;

	inputs_init(&application->inputs);
	TracyCZoneN(sdli, "SDL_Init", true);
	SDL_InitSubSystem(SDL_INIT_GAMEPAD);
	TracyCZoneEnd(sdli);
	TracyCZoneN(sdlcw, "SDL_CreateWindow", true);
	application->window = SDL_CreateWindow("tek", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	TracyCZoneEnd(sdlcw);

	load_assets(&application->assets, application->renderer);

	ImGui_CreateContext(NULL);
	ImGuiPlatformIO* platform_io = ImGui_GetPlatformIO();
	platform_io->Renderer_RenderState = application;
	// platform_io->Platform_SetClipboardTextFn = ImGui_ImplSDL3_SetClipboardText;
	// platform_io->Platform_GetClipboardTextFn = ImGui_ImplSDL3_GetClipboardText;
	platform_io->Platform_SetImeDataFn = ImGui_ImplSDL3_PlatformSetImeData;
	// platform_io->Platform_OpenInShellFn = [](ImGuiContext*, const char* url) { return SDL_OpenURL(url) == 0; };
	ImGuiViewport* viewport = ImGui_GetMainViewport();
	viewport->PlatformHandle = (void*)(intptr_t)SDL_GetWindowID(application->window);

	application->renderer = calloc(1, renderer_get_size());
	renderer_init(application->renderer, &application->assets, application->window);

	application->drawer = calloc(1, sizeof(struct Drawer2D));
	drawer2d_init(application->drawer, application->renderer);

	int display_w, display_h;
	SDL_GetWindowSizeInPixels(application->window, &display_w, &display_h);
	clay_integration_init(application->drawer, display_w, display_h);

	postload_assets(&application->assets, application->renderer);

	application->game.assets = &application->assets;
	application->game.renderer = application->renderer;
	application->game.inputs = &application->inputs;
	game_init(&application->game);

	watcher_init("cooking");

	TracyCZoneEnd(f);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	struct Application *application = appstate;
}

ImGuiKey ImGui_ImplSDL3_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode)
{
    // Keypad doesn't have individual key values in SDL3
    switch (scancode)
    {
        case SDL_SCANCODE_KP_0: return ImGuiKey_Keypad0;
        case SDL_SCANCODE_KP_1: return ImGuiKey_Keypad1;
        case SDL_SCANCODE_KP_2: return ImGuiKey_Keypad2;
        case SDL_SCANCODE_KP_3: return ImGuiKey_Keypad3;
        case SDL_SCANCODE_KP_4: return ImGuiKey_Keypad4;
        case SDL_SCANCODE_KP_5: return ImGuiKey_Keypad5;
        case SDL_SCANCODE_KP_6: return ImGuiKey_Keypad6;
        case SDL_SCANCODE_KP_7: return ImGuiKey_Keypad7;
        case SDL_SCANCODE_KP_8: return ImGuiKey_Keypad8;
        case SDL_SCANCODE_KP_9: return ImGuiKey_Keypad9;
        case SDL_SCANCODE_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDL_SCANCODE_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDL_SCANCODE_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDL_SCANCODE_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDL_SCANCODE_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDL_SCANCODE_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDL_SCANCODE_KP_EQUALS: return ImGuiKey_KeypadEqual;
        default: break;
    }
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        //case SDLK_APOSTROPHE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        //case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        //case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        //case SDLK_EQUALS: return ImGuiKey_Equal;
        //case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        //case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        //case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        //case SDLK_GRAVE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_A: return ImGuiKey_A;
        case SDLK_B: return ImGuiKey_B;
        case SDLK_C: return ImGuiKey_C;
        case SDLK_D: return ImGuiKey_D;
        case SDLK_E: return ImGuiKey_E;
        case SDLK_F: return ImGuiKey_F;
        case SDLK_G: return ImGuiKey_G;
        case SDLK_H: return ImGuiKey_H;
        case SDLK_I: return ImGuiKey_I;
        case SDLK_J: return ImGuiKey_J;
        case SDLK_K: return ImGuiKey_K;
        case SDLK_L: return ImGuiKey_L;
        case SDLK_M: return ImGuiKey_M;
        case SDLK_N: return ImGuiKey_N;
        case SDLK_O: return ImGuiKey_O;
        case SDLK_P: return ImGuiKey_P;
        case SDLK_Q: return ImGuiKey_Q;
        case SDLK_R: return ImGuiKey_R;
        case SDLK_S: return ImGuiKey_S;
        case SDLK_T: return ImGuiKey_T;
        case SDLK_U: return ImGuiKey_U;
        case SDLK_V: return ImGuiKey_V;
        case SDLK_W: return ImGuiKey_W;
        case SDLK_X: return ImGuiKey_X;
        case SDLK_Y: return ImGuiKey_Y;
        case SDLK_Z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        case SDLK_F13: return ImGuiKey_F13;
        case SDLK_F14: return ImGuiKey_F14;
        case SDLK_F15: return ImGuiKey_F15;
        case SDLK_F16: return ImGuiKey_F16;
        case SDLK_F17: return ImGuiKey_F17;
        case SDLK_F18: return ImGuiKey_F18;
        case SDLK_F19: return ImGuiKey_F19;
        case SDLK_F20: return ImGuiKey_F20;
        case SDLK_F21: return ImGuiKey_F21;
        case SDLK_F22: return ImGuiKey_F22;
        case SDLK_F23: return ImGuiKey_F23;
        case SDLK_F24: return ImGuiKey_F24;
        case SDLK_AC_BACK: return ImGuiKey_AppBack;
        case SDLK_AC_FORWARD: return ImGuiKey_AppForward;
        default: break;
    }

    // Fallback to scancode
    switch (scancode)
    {
    case SDL_SCANCODE_GRAVE: return ImGuiKey_GraveAccent;
    case SDL_SCANCODE_MINUS: return ImGuiKey_Minus;
    case SDL_SCANCODE_EQUALS: return ImGuiKey_Equal;
    case SDL_SCANCODE_LEFTBRACKET: return ImGuiKey_LeftBracket;
    case SDL_SCANCODE_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDL_SCANCODE_NONUSBACKSLASH: return ImGuiKey_Oem102;
    case SDL_SCANCODE_BACKSLASH: return ImGuiKey_Backslash;
    case SDL_SCANCODE_SEMICOLON: return ImGuiKey_Semicolon;
    case SDL_SCANCODE_APOSTROPHE: return ImGuiKey_Apostrophe;
    case SDL_SCANCODE_COMMA: return ImGuiKey_Comma;
    case SDL_SCANCODE_PERIOD: return ImGuiKey_Period;
    case SDL_SCANCODE_SLASH: return ImGuiKey_Slash;
    default: break;
    }
    return ImGuiKey_None;
}

static void ImGui_ImplSDL3_UpdateKeyModifiers(SDL_Keymod sdl_key_mods)
{
    ImGuiIO* io = ImGui_GetIO();
    ImGuiIO_AddKeyEvent(io, ImGuiMod_Ctrl, (sdl_key_mods & SDL_KMOD_CTRL) != 0);
    ImGuiIO_AddKeyEvent(io, ImGuiMod_Shift, (sdl_key_mods & SDL_KMOD_SHIFT) != 0);
    ImGuiIO_AddKeyEvent(io, ImGuiMod_Alt, (sdl_key_mods & SDL_KMOD_ALT) != 0);
    ImGuiIO_AddKeyEvent(io, ImGuiMod_Super, (sdl_key_mods & SDL_KMOD_GUI) != 0);
}

static void ImGui_ImplSDL3_PlatformSetImeData(ImGuiContext* ctx, ImGuiViewport* viewport, ImGuiPlatformImeData* data)
{
	// platform_io->Renderer_RenderState = application;
	ImGuiIO* io = ImGui_GetIO();
	SDL_WindowID window_id = (SDL_WindowID)(intptr_t)viewport->PlatformHandle;
	SDL_Window* window = SDL_GetWindowFromID(window_id);

	if (!(data->WantVisible || io->WantTextInput)) {
		SDL_StopTextInput(window);
	}
	if (data->WantVisible) {
		SDL_Rect r;
		r.x = (int)data->InputPos.x;
		r.y = (int)data->InputPos.y;
		r.w = 1;
		r.h = (int)data->InputLineHeight;
		SDL_SetTextInputArea(window, &r, 0);
	}
	if (data->WantVisible || io->WantTextInput)
		SDL_StartTextInput(window);
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

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
		{
			ImGui_ImplSDL3_UpdateKeyModifiers((SDL_Keymod)event->key.mod);
			//IMGUI_DEBUG_LOG("SDL_EVENT_KEY_%s : key=%d ('%s'), scancode=%d ('%s'), mod=%X\n",
			//    (event->type == SDL_EVENT_KEY_DOWN) ? "DOWN" : "UP  ", event->key.key, SDL_GetKeyName(event->key.key), event->key.scancode, SDL_GetScancodeName(event->key.scancode), event->key.mod);
			ImGuiKey key = ImGui_ImplSDL3_KeyEventToImGuiKey(event->key.key, event->key.scancode);
			ImGuiIO_AddKeyEvent(io, key, (event->type == SDL_EVENT_KEY_DOWN));
			ImGuiIO_SetKeyEventNativeData(io, key, event->key.key, event->key.scancode); // To support legacy indexing (<1.87 user code). Legacy backend uses SDLK_*** as indices to IsKeyXXX() functions.
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
	TracyCZoneN(f, "AppEvent", true);

	struct Application *application = appstate;
	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}

	imgui_process_event(application, event);
	inputs_process_event(event, &application->inputs);
	clay_integration_process_event(application, event);

	TracyCZoneEnd(f);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	TracyCZoneN(appiterate, "MainLoop", true);

	struct Application *application = appstate;

	// hot-reload materials
	bool atleast_one_change = watcher_tick();
	if (atleast_one_change) {
		load_assets_materials(&application->assets, application->renderer);
		renderer_init_materials(application->renderer, &application->assets);
	}

	 inputs_begin_frame(&application->inputs);

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

	drawer2d_reset_frame(application->drawer);
	ui_new_frame();

	// Setup clay size
        // Optional: Update internal layout dimensions to support resizing
        Clay_SetLayoutDimensions((Clay_Dimensions) { display_w, display_h });
        // All clay layouts are declared between Clay_BeginLayout and Clay_EndLayout
        Clay_BeginLayout();

	uint64_t new_time = SDL_GetTicks();
	uint64_t previous_frame_time = new_time - application->current_time;
	application->current_time = new_time;

	// inputs_imgui(&application->inputs);

	struct GameUpdateContext update_ctx = {};
	update_ctx.inputs = application->inputs;
	update_ctx.current_time = application->current_time;
	update_ctx.previous_frame_time = previous_frame_time;
	update_ctx.f = application->f;
	game_update(&application->game, &update_ctx);

	game_render(&application->game);

        Clay_RenderCommandArray clay_render_commands = Clay_EndLayout();
	clay_integration_render(application->drawer, &clay_render_commands);
	application->drawer->viewport_width = display_w;
	application->drawer->viewport_height = display_h;
	renderer_set_drawer2d(application->renderer, application->drawer);

	renderer_set_time(application->renderer, ((float)application->current_time) / 1000.0f);

	renderer_render(application->renderer);

	application->f += 1;
	TracyCZoneEnd(appiterate);
	TracyCFrameMark;
	return SDL_APP_CONTINUE;
}

#include "asset.c"
#include "vulkan.c"
#include "debugdraw.c"
#include "renderer.c"
#include "game.c"
#include "game_battle.c"
#include "game_mainmenu.c"
#include "game_local_battle.c"
#include "game_network_battle.c"
#include "inputs.c"
#include "anim.c"
#include "game_components.c"
#include "tek.c"
#include <ufbx.c>
#include "watcher.c"
#include "clay_integration.c"
#include "drawer2d.c"
#include "atlas2d.c"
#include "ui.c"

#include "editor.c"
