#include "inputs.h"

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
	if (is_button_up || is_button_down) {
		if (event->key.scancode == SDL_SCANCODE_W) {
			inputs->buttons_is_pressed[InputButtons_W] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_A) {
			inputs->buttons_is_pressed[InputButtons_A] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_S) {
			inputs->buttons_is_pressed[InputButtons_S] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_D) {
			inputs->buttons_is_pressed[InputButtons_D] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_U) {
			inputs->buttons_is_pressed[InputButtons_U] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_I) {
			inputs->buttons_is_pressed[InputButtons_I] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_O) {
			inputs->buttons_is_pressed[InputButtons_O] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_P) {
			inputs->buttons_is_pressed[InputButtons_P] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_J) {
			inputs->buttons_is_pressed[InputButtons_J] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_K) {
			inputs->buttons_is_pressed[InputButtons_K] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_L) {
			inputs->buttons_is_pressed[InputButtons_L] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_SEMICOLON) {
			inputs->buttons_is_pressed[InputButtons_SEMICOLON] = is_button_down;
		} else if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
			inputs->buttons_is_pressed[InputButtons_Escape] = is_button_down;
		}

	}

	bool is_gamepad_down = event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
	bool is_gamepad_up = event->type == SDL_EVENT_GAMEPAD_BUTTON_UP;
	if (is_gamepad_down || is_gamepad_up) {
		assert((SDL_GamepadButton)InputGamepadButtons_COUNT == SDL_GAMEPAD_BUTTON_COUNT);

		SDL_JoystickID which = event->gbutton.which;
		inputs->gamepad_buttons_is_pressed[event->gbutton.button] = is_gamepad_down;
	}


	if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		inputs->is_mouse_down = true;
	} else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
		inputs->is_mouse_down = false;
	}

	return true;
}
