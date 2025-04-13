#pragma once

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

enum tek_PlayerStatesBits
{
	TEK_PLAYER_STATE_INTANGIBLE  = (1 << 0), // nothing can hit
	TEK_PLAYER_STATE_HIGH_CRUSH  = (1 << 1), // highs can't hit
	TEK_PLAYER_STATE_LOW_CRUSH   = (1 << 2), // lows can't hit
	TEK_PLAYER_STATE_AIRBORNE    = (1 << 3), // hits float player
	TEK_PLAYER_STATE_POWER_CRUSH = (1 << 4), // highs and mids are absorbed, but still deal damage
	TEK_PLAYER_STATE_PARRY       = (1 << 5), // specific attacks are absorbed
	TEK_PLAYER_STATE_RAGE_ARMOR  = (1 << 6), // all attacks are absorbed
	TEK_PLAYER_STATE_ARMOR       = (1 << 7), // powercrush?
};
typedef uint8_t tek_PlayerState;

enum tek_AttackType
{
	TEK_ATTACK_TYPE_HIGH = 0,
	TEK_ATTACK_TYPE_MID = 0,
	TEK_ATTACK_TYPE_LOW = 0,
	TEK_ATTACK_TYPE_SPECIAL_LOW = 0, // can be block standing and crouching
};

struct tek_Move
{
	// animation
	uint32_t animation_id;
	// frame data
	uint8_t startup; // how long is the startup of the animation
	uint8_t active; // how long is the move active
	uint8_t recovery; // how long is the recovery
	uint8_t hitstun; // stun the opponent for X frames on hit
	uint8_t blockstuck; // stun the opponent for X frames on block
	// gameplay
	uint8_t base_damage;
	tek_MotionInput motion_input;
	uint8_t action_input;
	// stance transitions
	tek_MotionInput transition_input;
	uint32_t transition_stance_id;
};

struct tek_Stance
{
	uint32_t id;
	struct tek_Move moves[128];
	uint32_t moves_length;
};

struct tek_Character
{
	struct tek_Stance stances[8];
	uint32_t stances_length;
	// 3d model
};


// ====
struct tek_Character characters[1] = {0};
