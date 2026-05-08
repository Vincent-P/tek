#pragma once

#include "game_mainmenu.h"
#include "game_local_battle.h"
#include "game_network_battle.h"


struct AssetLibrary;
struct Renderer;
struct Inputs;
typedef uint64_t SteamAPICall_t;


enum GameState
{
	GAME_STATE_MAIN_MENU = 0,
	GAME_STATE_LOCAL_BATTLE,
	GAME_STATE_NETWORK_BATTLE,
};

struct Game
{
	struct AssetLibrary *assets;
	struct Renderer *renderer;
	struct Inputs const*inputs;

	// steam
	SteamAPICall_t lobby_join_request;
	uint64_t lobby_id;

	// common state
	struct Simulation
	{
		uint64_t accumulator;
		uint64_t t;
		struct BattleContext battle_context;
	} simulation;

	// state
	enum GameState current_state;
	struct MainMenu mainmenu;
	struct LocalBattle local_battle;
	struct NetworkBattle network_battle;
};

struct GameUpdateContext
{
	struct Inputs inputs;
	uint64_t current_time;
	uint64_t previous_frame_time;
	uint64_t f;
};


void game_first_init(struct Game *game); // 1st line of the main, called before any other init
void game_init(struct Game *game);
void game_update(struct Game *game, struct GameUpdateContext const* ctx);
void game_render(struct Game *game);
