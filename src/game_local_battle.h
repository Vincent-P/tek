#pragma once
#include "game_battle.h"

#define REPLAY_LENGTH_IN_FRAMES (60*60*60) // 1 hour

enum LocalBattleState
{
	LOCAL_BATTLE_STATE_PLAY,
	LOCAL_BATTLE_STATE_REPLAY,
	LOCAL_BATTLE_STATE_PAUSE,
	LOCAL_BATTLE_STATE_END,
};

enum LocalBattlePlayState
{
	LOCAL_BATTLE_PLAY_STATE_PLAYING,
	LOCAL_BATTLE_PLAY_STATE_RECORDING,
};


enum LocalBattleReplayState
{
	LOCAL_BATTLE_REPLAY_STATE_WATCHING,
	LOCAL_BATTLE_REPLAY_STATE_PAUSED,
};


struct LocalBattle
{

	// States
	enum LocalBattleState state;
	enum LocalBattlePlayState play_state;
	enum LocalBattleReplayState replay_state;

	// play recording state
	struct BattleContext replay_initial_context;
	struct BattleInputs replay_inputs[60*60*60];
	uint32_t replay_current_input;
	uint32_t replay_length;
	// replay watching state
	uint32_t watching_frame;
	// pause state
	enum LocalBattleState pause_previous_state;

	// UI data
	bool pause_resume_pressed;
	bool pause_options_pressed;
	bool pause_mainmenu_pressed;
	bool end_rematch_pressed;
	bool end_mainmenu_pressed;
};


void local_battle_init(struct Game *game);
void local_battle_term(struct Game *game);
bool local_battle_update(struct Game *game, struct GameUpdateContext const *ctx);
void local_battle_render(struct Game *game);
