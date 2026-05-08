#pragma once
#include "game_battle.h"

#define REPLAY_LENGTH_IN_FRAMES (60*60*60) // 1 hour

enum LocalBattleState
{
	LOCAL_BATTLE_STATE_PLAYING, // LocalBattlePlayingState
	LOCAL_BATTLE_STATE_WATCHING, // ReplayWatcherState
	LOCAL_BATTLE_STATE_PAUSE, // Pause menu
	LOCAL_BATTLE_STATE_END, // End of round
};

enum LocalBattlePlayingState
{
	LOCAL_BATTLE_PLAYING_STATE_PLAYING, //
	LOCAL_BATTLE_PLAYING_STATE_RECORDING, // ReplayRecorderState
};

enum ReplayRecorderState
{
	REPLAY_RECORDER_STATE_INACTIVE, // is playing? can we collapse states to simplify?
	REPLAY_RECORDER_STATE_START_RECORD,
	REPLAY_RECORDER_STATE_ACTIVE,
	REPLAY_RECORDER_STATE_STOP_RECORD,
};

enum ReplayWatcherState
{
	REPLAY_WATCHER_STATE_START_PLAY,
	REPLAY_WATCHER_STATE_PLAYING,
	REPLAY_WATCHER_STATE_PAUSED,
	REPLAY_WATCHER_STATE_EXIT,
};


struct LocalBattle
{
	// External data
	// NOTE: The battle context has direct access to assets and renderer.
	// But to support rollback easily, inputs are converted and passed explicitly to the simulation.
	struct Inputs *inputs;
	struct Game const *game;

	// Battle data
	uint64_t accumulator;
	uint64_t t;
	struct BattleContext battle_context;

	// Replay data
	struct BattleContext replay_initial_context;
	struct BattleInputs replay_inputs[60*60*60];
	uint32_t replay_current_input;
	uint32_t replay_length;

	// State data
	enum LocalBattleState state;
	// playing state
	enum LocalBattlePlayingState playing_state;
	enum ReplayRecorderState replay_recorder_state;
	// watching state
	enum ReplayWatcherState replay_watcher_state;
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
void local_battle_term(struct LocalBattle *local_battle);
bool local_battle_update(struct Game *game, struct GameUpdateContext const *ctx);
void local_battle_render(struct LocalBattle *local_battle);
