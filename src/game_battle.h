#pragma once
#include "tek.h"
#include "game_components.h"

#define INPUT_BUFFER_SIZE 128 // Number of different inputs in the in buffer

struct Inputs;
typedef struct Renderer Renderer;

// Inputs for the battle system
enum BattleInputBits
{
	BATTLE_INPUT_BACK      = (1 << 0),
	BATTLE_INPUT_UP        = (1 << 1),
	BATTLE_INPUT_FORWARD   = (1 << 2),
	BATTLE_INPUT_DOWN      = (1 << 3),
	BATTLE_INPUT_LPUNCH    = (1 << 4),
	BATTLE_INPUT_RPUNCH    = (1 << 5),
	BATTLE_INPUT_LKICK     = (1 << 6),
	BATTLE_INPUT_RKICK     = (1 << 7),
};
struct BattleInput
{
	tek_MotionInput motion;
	tek_ActionInputSet actions;
};

struct BattleInputs
{
	struct BattleInput player1;
	struct BattleInput player2;
};

// Component for the battle system
struct TekPlayerComponent
{
	// character
	uint32_t character_id;
	// status
	uint32_t requested_move_id;
	uint32_t current_move_id;
	int hp;
	// pushback
	int pushback_remaining_frames;
	float pushback_strength;
	// input buffer
	struct BattleInput input_buffer[INPUT_BUFFER_SIZE];
	uint64_t input_buffer_head;
};

// Replicated player state
struct PlayerEntity
{
	struct SpatialComponent spatial;
	struct SkeletonComponent anim_skeleton;
	struct AnimationComponent animation;
	struct SkeletalMeshComponent mesh;
	struct TekPlayerComponent tek;
};

// Non-replicated player state
struct PlayerNonEntity
{
	struct SkeletalMeshInstance mesh_instance; // created from the renderer during init
	struct AnimPose pose; // technically is game state, but because it's computed each tick, it should be deterministic from the AnimationComponent
	Float3 hurtboxes_position[MAX_BONES_PER_MESH];
	Float3 hitboxes_position[MAX_HITBOXES];
};

// Replicated game state, this struct can be rollbacked in network
struct BattleState
{
	uint32_t frame_number;
	struct PlayerEntity p1_entity;
	struct PlayerEntity p2_entity;
};

// Non-replicated game state
struct BattleNonState
{
	// rounds
	int rounds_first_to; // number of rounds needed to win
	int rounds_p1_won; // number of won rounds for P1
	int rounds_p2_won; // number of won rounds for P2
	// game
	struct PlayerNonEntity p1_nonentity;
	struct PlayerNonEntity p2_nonentity;
	// rendering
	Camera camera;
	Camera camera_display;
	float camera_distance;
	int camera_focus; // 0 = none, 1 = p1, 2 = p2
	bool draw_grid;
	bool draw_hurtboxes;
	bool draw_hitboxes;
	bool draw_colisions;
	bool draw_bones;
	// player handles
	// session connection
};

// Main object for the battle system
struct BattleContext
{
	// external systems
	Renderer *renderer;
	struct AssetLibrary *assets;
	// battle state
	struct BattleState battle_state;
	struct BattleNonState battle_non_state;
};

enum BattleFrameResult
{
	BATTLE_FRAME_RESULT_CONTINUE,
	BATTLE_FRAME_RESULT_END,
};

void battle_state_init(struct BattleContext *ctx);
void battle_state_new_round(struct BattleContext *ctx);
void battle_state_term(struct BattleContext *ctx);

// GGPO requires a function to simulate 1 frame with specified inputs for rollback.
struct BattleInputs battle_read_input(struct Inputs const *inputs);
enum BattleFrameResult battle_simulate_frame(struct BattleContext *ctx, struct BattleInputs input);

// Update renderer with the latest game state.
void battle_render(struct BattleContext *ctx);
