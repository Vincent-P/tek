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

enum InputGamepadButtons
{
    InputGamepadButtons_SOUTH,           /**< Bottom face button (e.g. Xbox A button) */
    InputGamepadButtons_EAST,            /**< Right face button (e.g. Xbox B button) */
    InputGamepadButtons_WEST,            /**< Left face button (e.g. Xbox X button) */
    InputGamepadButtons_NORTH,           /**< Top face button (e.g. Xbox Y button) */
    InputGamepadButtons_BACK,
    InputGamepadButtons_GUIDE,
    InputGamepadButtons_START,
    InputGamepadButtons_LEFT_STICK,
    InputGamepadButtons_RIGHT_STICK,
    InputGamepadButtons_LEFT_SHOULDER,
    InputGamepadButtons_RIGHT_SHOULDER,
    InputGamepadButtons_DPAD_UP,
    InputGamepadButtons_DPAD_DOWN,
    InputGamepadButtons_DPAD_LEFT,
    InputGamepadButtons_DPAD_RIGHT,
    InputGamepadButtons_MISC1,           /**< Additional button (e.g. Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button, Google Stadia capture button) */
    InputGamepadButtons_RIGHT_PADDLE1,   /**< Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1) */
    InputGamepadButtons_LEFT_PADDLE1,    /**< Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3) */
    InputGamepadButtons_RIGHT_PADDLE2,   /**< Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle P2) */
    InputGamepadButtons_LEFT_PADDLE2,    /**< Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4) */
    InputGamepadButtons_TOUCHPAD,        /**< PS4/PS5 touchpad button */
    InputGamepadButtons_MISC2,           /**< Additional button */
    InputGamepadButtons_MISC3,           /**< Additional button */
    InputGamepadButtons_MISC4,           /**< Additional button */
    InputGamepadButtons_MISC5,           /**< Additional button */
    InputGamepadButtons_MISC6,           /**< Additional button */
    InputGamepadButtons_COUNT
};

#define INPUTS_GAMEPAD_CAPACITY 4

struct Inputs
{
	SDL_Gamepad *gamepads[INPUTS_GAMEPAD_CAPACITY];
	uint32_t gamepads_count;

	bool gamepad_buttons_is_pressed[InputGamepadButtons_COUNT];


	bool buttons_is_pressed[InputButtons_Count];
	bool is_mouse_down;
};

void inputs_init(struct Inputs *inputs);
bool inputs_process_event(SDL_Event *event, struct Inputs *inputs);
