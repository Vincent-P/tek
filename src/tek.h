#pragma once

// -- Runtime data

enum tek_ActionStateType
{
	TEK_ACTION_STATE_NONE     = 0, // no action
	TEK_ACTION_STATE_STARTUP  = 1, // startup, transition to active
	TEK_ACTION_STATE_ACTIVE   = 2, // active, transition to recovery
	TEK_ACTION_STATE_RECOVERY = 3, // recovery, transition to none
};

struct tek_ActionState
{
	uint8_t type; // current state
	uint8_t end;  // last frame of the effect if not 0
};

// -- Static data

enum tek_MotionInputBits
{
	TEK_MOTION_INPUT_NONE  = 0,
	TEK_MOTION_INPUT_DB    = 1,
	TEK_MOTION_INPUT_B     = 2,
	TEK_MOTION_INPUT_UB    = 3,
	TEK_MOTION_INPUT_U     = 4,
	TEK_MOTION_INPUT_UF    = 5,
	TEK_MOTION_INPUT_F     = 6,
	TEK_MOTION_INPUT_DF    = 7,
	TEK_MOTION_INPUT_D     = 8,
	TEK_MOTION_INPUT_F_QCF = 9,
	TEK_MOTION_INPUT_QCF   = 10,
	TEK_MOTION_INPUT_QCB   = 11,
};
typedef uint8_t tek_MotionInput;

enum tek_AttackType
{
	TEK_ATTACK_TYPE_HIGH = 0,
	TEK_ATTACK_TYPE_MID = 0,
	TEK_ATTACK_TYPE_LOW = 0,
	TEK_ATTACK_TYPE_SPECIAL_LOW = 0, // can be block standing and crouching
};

struct tek_Move
{
	uint32_t id;
	// animation
	uint32_t animation_id;
	// frame data
	uint8_t startup; // how long is the startup of the animation
	uint8_t active; // how long is the move active
	uint8_t recovery; // how long is the recovery
	uint8_t hitbox; // index in hitbox list
	// hit reaction, TODO: reactions list to handle airborne, CH, etc
	uint8_t base_damage;
	uint8_t hitstun; // stun the opponent for X frames on hit
	uint8_t blockstun; // stun the opponent for X frames on block
};

struct tek_Cancel
{
	uint32_t from_move_id;
	uint32_t to_move_id;
	tek_MotionInput motion_input;
	uint8_t action_input;
	// input window
};

#define MAX_HITBOXES 8
struct tek_Character
{
	uint32_t id;
	uint32_t skeletal_mesh_id;
	uint32_t anim_skeleton_id;
	// moves
	struct tek_Move moves[128];
	struct tek_Cancel cancels[128];
	uint32_t moves_length;
	uint32_t cancels_length;
	// hurtboxes, 0 or 1 per bone
	float hurtboxes_radius[MAX_BONES_PER_MESH];
	float hurtboxes_height[MAX_BONES_PER_MESH];
	uint32_t hurtboxes_bone_id[MAX_BONES_PER_MESH];
	uint32_t hurtboxes_length;
	// hitboxes, referrenced by index in this list
	float hitboxes_radius[MAX_HITBOXES];
	float hitboxes_height[MAX_HITBOXES];
	uint32_t hitboxes_bone_id[MAX_HITBOXES];
	uint32_t hitboxes_length;
};


// ====
struct tek_Character tek_characters[1] = {0};
const char* tek_characters_filename[1] = {"michel.character.json"};

void tek_read_character_json();
