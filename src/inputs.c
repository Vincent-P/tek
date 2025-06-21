#include "inputs.h"

bool inputs_process_event(SDL_Event *event, struct Inputs *inputs)
{
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
		} 

	}

	if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		inputs->is_mouse_down = true;
	} else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
		inputs->is_mouse_down = false;
	}

	return true;
}
