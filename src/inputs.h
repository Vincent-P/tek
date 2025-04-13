#pragma once

enum InputButtons
{
	InputButtons_W,
	InputButtons_A,
	InputButtons_S,
	InputButtons_D,
	InputButtons_U,
	InputButtons_I,
	InputButtons_O,
	InputButtons_P,
	InputButtons_J,
	InputButtons_K,
	InputButtons_L,
	InputButtons_SEMICOLON,
	InputButtons_Count,
};

struct Inputs
{
	bool buttons_is_pressed[InputButtons_Count];
};

bool inputs_process_event(SDL_Event *event, struct Inputs *inputs);
