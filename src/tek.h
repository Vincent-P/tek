#pragma once

#define MAX_CANCELS_PER_MOVE 8
#define MAX_HIT_CONDITIONS_PER_MOVE 8
#define MAX_CANCELS_PER_GROUP 32
#define MAX_CANCEL_GROUPS 16
#define MAX_HITBOXES 8

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
/** Directional input, following numpad notation order. **/
enum tek_MotionInputBits
{
	TEK_MOTION_INPUT_ANY     = 0,
	TEK_MOTION_INPUT_DB      = 1,
	TEK_MOTION_INPUT_D       = 2,
	TEK_MOTION_INPUT_DF      = 3,
	TEK_MOTION_INPUT_B       = 4,
	TEK_MOTION_INPUT_N       = 5,
	TEK_MOTION_INPUT_F       = 6,
	TEK_MOTION_INPUT_UB      = 7,
	TEK_MOTION_INPUT_U       = 8,
	TEK_MOTION_INPUT_UF      = 9,
};
typedef uint8_t tek_MotionInput;
typedef uint16_t tek_MotionInputSet;

/** Action input, following numpad notation order. **/
enum tek_ActionInputBits
{
	TEK_ACTION_INPUT_LP      = 0,
	TEK_ACTION_INPUT_RP      = 1,
	TEK_ACTION_INPUT_LK      = 2,
	TEK_ACTION_INPUT_RK      = 3
};
typedef uint8_t tek_ActionInput;
typedef uint8_t tek_ActionInputSet;

enum tek_ButtonCommandType
{
	TEK_BUTTON_COMMAND_INVALID     = 0,
	TEK_BUTTON_COMMAND_PARTIAL     = 1, // Accept any of the specified buttons
	TEK_BUTTON_COMMAND_NORMAL      = 2, // Accept at least the specified buttons
	TEK_BUTTON_COMMAND_DIRECTION   = 3, // Accept only a direction (no actions allowed)
};
typedef uint8_t tek_ButtonCommandType;

enum tek_BuiltinCommands
{
	TEK_BUILTIN_COMMAND_FF,
	TEK_BUILTIN_COMMAND_BB,
	TEK_BUILTIN_COMMAND_QCF,
	TEK_BUILTIN_COMMAND_QCB,
};

union tek_Command
{
	struct
	{
		tek_MotionInputSet motion;
		uint16_t padding;
		struct
		{
			tek_ButtonCommandType type;
			tek_ActionInputSet not_held;
			tek_ActionInputSet held;
			tek_ActionInputSet pressed;
		} action;
	} fields;
	uint64_t raw;
};

enum tek_AttackType
{
	TEK_ATTACK_TYPE_HIGH = 0,
	TEK_ATTACK_TYPE_MID = 0,
	TEK_ATTACK_TYPE_LOW = 0,
	TEK_ATTACK_TYPE_SPECIAL_LOW = 0, // can be block standing and crouching
};

enum tek_CancelType
{
	TEK_CANCEL_TYPE_SINGLE = 0,
	TEK_CANCEL_TYPE_SINGLE_LOOP, // single cancel, that loops on itself so we have to keep the animation going
	TEK_CANCEL_TYPE_SINGLE_CONTINUE, // single cancel, keep the animation frame from the previous move
	TEK_CANCEL_TYPE_LIST, // list cancel, points to a cancel group
	TEK_CANCEL_TYPE_COUNT
};
typedef uint8_t tek_CancelType;

enum tek_CancelCondition
{
	TEK_CANCEL_CONDITION_TRUE = 0,
	TEK_CANCEL_CONDITION_END_OF_ANIMATION,
	TEK_CANCEL_CONDITION_COUNT,
};
typedef uint8_t tek_CancelCondition;

struct tek_Cancel
{
	uint32_t to_move_id; // cancel group index if type is list
	union tek_Command command;
	tek_CancelType type;
	tek_CancelCondition condition;
	uint8_t input_window_start; // relative to the current move
	uint8_t input_window_end; //
	uint8_t starting_frame; // at which frame of the current move the cancel should be applied (does not read input?)
};

struct tek_CancelGroup
{
	uint32_t id;
	struct tek_Cancel cancels[MAX_CANCELS_PER_GROUP];
	uint32_t cancels_length;
};

struct tek_HitCondition
{
	uint32_t reactions_id;
	uint8_t damage;
	uint8_t requirements;
};

enum tek_HitLevel
{
	TEK_HIT_LEVEL_NONE = 0,
	TEK_HIT_LEVEL_HIGH,
	TEK_HIT_LEVEL_MID,
	TEK_HIT_LEVEL_LOW,
	TEK_HIT_LEVEL_UNBLOCKABLE,
	TEK_HIT_LEVEL_COUNT,
};

enum tek_BuiltinMoves
{
	TEK_MOVE_IDLE_ID           = 2648613821,
	TEK_MOVE_FORWARD_START_ID  = 2063425625,
	TEK_MOVE_FORWARD_LOOP_ID   = 2390309482,
	TEK_MOVE_FORWARD_DASH_ID   = 2785191083,
	TEK_MOVE_BACKWARD_LOOP_ID  = 2224257066,
	TEK_MOVE_BACKWARD_DASH_ID  = 3746350146,
	TEK_MOVE_DEATH_ID          = 3920613624,
	TEK_MOVE_STANDING_BLOCK_ID = 2813135088,
	TEK_MOVE_CROUCH_BLOCK_ID   =  525369408,
	TEK_MOVE_WHILE_STANDING_ID =  505395132,
};


struct tek_Move
{
	uint32_t id;
	// animation
	uint32_t animation_id;
	// hit
	enum tek_HitLevel hit_level;
	uint8_t hitbox; // index in hitbox list
	// frame data
	uint8_t first_active;
	uint8_t last_active;
	// cancels
	struct tek_Cancel cancels[MAX_CANCELS_PER_MOVE];
	uint32_t cancels_length;
	// hit conditions
	struct tek_HitCondition hit_conditions[MAX_HIT_CONDITIONS_PER_MOVE];
	uint32_t hit_conditions_length;
};

struct tek_HitReactions
{
	uint32_t id;
	uint32_t standing_move;
	uint32_t standing_counter_hit_move;
	uint32_t standing_block_move;
	uint32_t crouch_block_move;
};

struct tek_DebugName
{
	char string[32];
};

struct tek_Character
{
	uint32_t id;
	uint32_t skeletal_mesh_id;
	uint32_t anim_skeleton_id;
	float colision_radius;
	int max_health;
	// moves
	struct tek_Move moves[128];
	uint32_t moves_length;
	struct tek_CancelGroup cancel_groups[MAX_CANCEL_GROUPS];
	uint32_t cancel_groups_length;
	// reactions
	struct tek_HitReactions hit_reactions[128];
	uint32_t hit_reactions_length;
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

	struct tek_DebugName move_names[128+1];
};


// ====
struct tek_Character tek_characters[1] = {0};
const char* tek_characters_filename[1] = {"michel.character.json"};

void tek_read_character_json();

struct tek_Move *tek_character_find_move(struct tek_Character *character, uint32_t id);
struct tek_CancelGroup *tek_character_find_cancel_group(struct tek_Character *character, uint32_t id);
struct tek_HitReactions *tek_character_find_hit_reaction(struct tek_Character *character, uint32_t id);
