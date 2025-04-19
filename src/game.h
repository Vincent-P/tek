#pragma once
#include "tek.h"

#define INPUT_BUFFER_SIZE 20

struct Inputs;
typedef struct ufbx_scene ufbx_scene;

// For GGPO, we need a single big object for the game state, and for inputs.
enum GameInputBits
{
	GAME_INPUT_BACK      = (1 << 0),
	GAME_INPUT_UP        = (1 << 1),
	GAME_INPUT_FORWARD   = (1 << 2),
	GAME_INPUT_DOWN      = (1 << 3),
	
	GAME_INPUT_LPUNCH    = (1 << 4),
	GAME_INPUT_RPUNCH    = (1 << 5),
	GAME_INPUT_LKICK     = (1 << 6),
	GAME_INPUT_RKICK     = (1 << 7),
};
typedef uint8_t GameInput;

struct GameInputs
{
	GameInput player1;
	GameInput player2;
};

struct PlayerState
{
	uint32_t character_id;
	Float3 position;
	tek_PlayerState status;
	GameInput input_buffer[INPUT_BUFFER_SIZE];
	uint32_t input_buffer_frame_start[INPUT_BUFFER_SIZE];
	uint32_t current_input_index;
};

struct GameState
{
	uint32_t frame_number;
	
	struct PlayerState player1;
	struct PlayerState player2;
};

struct NonGameState
{
	// frame
	uint32_t frame_number;
	float dt;
	float t;
	// game
	SkeletalMeshWithAnimationsAsset skeletal_mesh_with_animations;
	struct AnimPose p1_pose;
	// rendering
	Camera camera;
	// player handles
	// session connection
};

// GGPO requires a function to simulate 1 frame with specified inputs for rollback.
struct GameInputs game_read_input(struct Inputs *inputs);
void game_simulate_frame(struct NonGameState *ngs, struct GameState *state, struct GameInputs input);

void game_state_init(struct GameState *state);
void game_non_state_init(struct NonGameState *state);

// Called by the game during a rollback or a frame.
void game_state_update(struct NonGameState *nonstate, struct GameState *state, struct GameInputs input);

// Update renderer with the latest game state.
void game_render(struct NonGameState *nonstate, struct GameState *state);
