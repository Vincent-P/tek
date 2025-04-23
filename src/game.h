#pragma once
#include "tek.h"
#include "game_components.h"

#define INPUT_BUFFER_SIZE 20

struct Inputs;
typedef struct Renderer Renderer;

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

struct TekPlayerComponent
{
	uint32_t character_id;
	tek_PlayerState status;
	GameInput input_buffer[INPUT_BUFFER_SIZE];
	uint32_t input_buffer_frame_start[INPUT_BUFFER_SIZE];
	uint32_t current_input_index;
};

struct PlayerEntity
{
	struct SpatialComponent spatial;
	struct SkeletonComponent anim_skeleton;
	struct AnimationComponent animation;
	struct SkeletalMeshComponent mesh;
	struct TekPlayerComponent tek;
};

struct GameState
{
	uint32_t frame_number;
	struct PlayerEntity player1_entity;
	struct PlayerEntity player2_entity;
};

struct NonGameState
{
	struct AssetLibrary *assets;
	// frame
	uint32_t frame_number;
	float dt;
	float t;
	// game
	struct AnimPose p1_pose; // technically is game state, but because it's computed each tick, it should be deterministic from the AnimationComponent
	struct SkeletalMeshInstance p1_mesh_instance; // created from the renderer during init
	struct AnimPose p2_pose;
	struct SkeletalMeshInstance p2_mesh_instance;
	// rendering
	Renderer *renderer;
	Camera camera;
	// player handles
	// session connection
};

// GGPO requires a function to simulate 1 frame with specified inputs for rollback.
struct GameInputs game_read_input(struct Inputs *inputs);
void game_simulate_frame(struct NonGameState *ngs, struct GameState *state, struct GameInputs input);

void game_state_init(struct GameState *state);
void game_non_state_init(struct NonGameState *state, Renderer *renderer);

// Called by the game during a rollback or a frame.
void game_state_update(struct NonGameState *nonstate, struct GameState *state, struct GameInputs input);

// Update renderer with the latest game state.
void game_render(struct NonGameState *nonstate, struct GameState *state);
