#pragma once
#include "game_battle.h"
#include <ggponet.h>


typedef uint64_t SteamAPICall_t;


enum NetworkBattleState
{
	NETWORK_BATTLE_STATE_MAIN_MENU,
	// Host
	NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_CREATION,
	NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_PLAYERS,
	// Join
	NETWORK_BATTLE_STATE_CONNECTING_TO_LOBBY,
	// Common
	NETWORK_BATTLE_STATE_PLAY,
	NETWORK_BATTLE_STATE_END,
};


struct NetworkBattle
{
	// Steam data
	SteamAPICall_t lobby_create_call;
	uint64_t lobby_id;

	// Battle data
	uint64_t player_steam_ids[2];
	GGPOSession *ggpo_session;
	GGPOPlayer ggpo_players[2];
	GGPOPlayerHandle ggpo_player_handles[2];

	// State data
	enum NetworkBattleState state;

	// UI data
	bool host_pressed;
	bool join_pressed;
};


void network_battle_init(struct Game *game);
// joined through Steam overlay
void network_battle_on_lobby_joined(struct Game *game, uint64_t lobby_id);

void network_battle_term(struct Game *game);
bool network_battle_update(struct Game *game, struct GameUpdateContext const *ctx);
void network_battle_steam_callback(struct Game *game, struct GameUpdateContext const *ctx, int callback_type, void *callback_data, int callback_datasize);
void network_battle_render(struct Game *game);
