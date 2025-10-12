#include "inputs.h"

const char* InputButtons_str[InputButtons_Count] =
	{
		"W",
		"A",
		"S",
		"D",
		"U",
		"I",
		"O",
		"P",
		"J",
		"K",
		"L",
		"SEMICOLON",
		"Escape",
	};

void inputs_init(struct Inputs *inputs)
{

}

bool inputs_process_event(SDL_Event *event, struct Inputs *inputs)
{
	// gamepad management
	if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
		SDL_JoystickID gamepad_id = event->gdevice.which;
		SDL_Gamepad *gamepad = SDL_OpenGamepad(gamepad_id);

		printf("[inputs] gamepad added %x\n", gamepad_id);
	} else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
		SDL_JoystickID gamepad_id = event->gdevice.which;
		printf("[inputs] gamepad removed %x\n", gamepad_id);
	} else if (event->type == SDL_EVENT_GAMEPAD_REMAPPED) {
		SDL_JoystickID gamepad_id = event->gdevice.which;
		printf("[inputs] gamepad remapped %x\n", gamepad_id);
	}

	// inputs
	bool is_button_down = event->type ==  SDL_EVENT_KEY_DOWN;
	bool is_button_up = event->type ==  SDL_EVENT_KEY_UP;

#define UPDATE_BUTTON(X)					\
	if (inputs->buttons_ended_down[X] != is_button_down) {	\
		inputs->buttons_transitions_count[X] += 1;	\
		inputs->buttons_ended_down[X] = is_button_down; \
	}
	
	if (is_button_up || is_button_down) {
		if (event->key.scancode == SDL_SCANCODE_W) {
			UPDATE_BUTTON(InputButtons_W);
		} else if (event->key.scancode == SDL_SCANCODE_A) {
			UPDATE_BUTTON(InputButtons_A);
		} else if (event->key.scancode == SDL_SCANCODE_S) {
			UPDATE_BUTTON(InputButtons_S);
		} else if (event->key.scancode == SDL_SCANCODE_D) {
			UPDATE_BUTTON(InputButtons_D);
		} else if (event->key.scancode == SDL_SCANCODE_U) {
			UPDATE_BUTTON(InputButtons_U);
		} else if (event->key.scancode == SDL_SCANCODE_I) {
			UPDATE_BUTTON(InputButtons_I);
		} else if (event->key.scancode == SDL_SCANCODE_O) {
			UPDATE_BUTTON(InputButtons_O);
		} else if (event->key.scancode == SDL_SCANCODE_P) {
			UPDATE_BUTTON(InputButtons_P);
		} else if (event->key.scancode == SDL_SCANCODE_J) {
			UPDATE_BUTTON(InputButtons_J);
		} else if (event->key.scancode == SDL_SCANCODE_K) {
			UPDATE_BUTTON(InputButtons_K);
		} else if (event->key.scancode == SDL_SCANCODE_L) {
			UPDATE_BUTTON(InputButtons_L);
		} else if (event->key.scancode == SDL_SCANCODE_SEMICOLON) {
			UPDATE_BUTTON(InputButtons_SEMICOLON);
		} else if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
			UPDATE_BUTTON(InputButtons_Escape);
		}
	}

	bool is_gamepad_down = event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
	bool is_gamepad_up = event->type == SDL_EVENT_GAMEPAD_BUTTON_UP;
	if (is_gamepad_down || is_gamepad_up) {
		assert((SDL_GamepadButton)InputGamepadButtons_COUNT == SDL_GAMEPAD_BUTTON_COUNT);
		SDL_JoystickID which = event->gbutton.which;
		inputs->gamepad_buttons_is_down[event->gbutton.button] = is_gamepad_down;
	}


	if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		inputs->is_mouse_down = true;
	} else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
		inputs->is_mouse_down = false;
	}

	return true;
}

void inputs_begin_frame(struct Inputs *inputs)
{
	for (uint32_t ibutton = 0; ibutton < InputButtons_Count; ++ibutton) {
		inputs->buttons_is_down[ibutton] = inputs->buttons_ended_down[ibutton];
		inputs->buttons_was_pressed[ibutton] = inputs->buttons_ended_down[ibutton] && (inputs->buttons_transitions_count[ibutton] % 2 == 1);
		
		inputs->buttons_transitions_count[ibutton] = 0;
	}
}

void inputs_imgui(struct Inputs *inputs)
{
	for (uint32_t ibutton = 0; ibutton < InputButtons_Count; ++ibutton) {
		ImGui_Text("%s transitions: %u", InputButtons_str[ibutton], inputs->buttons_transitions_count[ibutton]);
		
		ImGui_Text("%s %s", InputButtons_str[ibutton], inputs->buttons_is_down[ibutton] ? "down" : "up");
		ImGui_Text("%s %s", InputButtons_str[ibutton], inputs->buttons_was_pressed[ibutton] ? "pressed" : "not pressed");
	}
}
