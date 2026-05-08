#pragma once
#include "game_battle.h"
#include <ggponet.h>


typedef uint64_t SteamAPICall_t;


enum NetworkBattleState
{
	NETWORK_BATTLE_STATE_MAIN_MENU, // Display Host/Join menu
	NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_CREATION, // Once player clicks "Host", a steam lobby is created, wait for async op to completed
	NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_PLAYERS, // when a steam lobby is created, wait for someone to join
	NETWORK_BATTLE_STATE_HOSTING, // Once someone joined, we create a GGPO session and start updating battle state
	NETWORK_BATTLE_STATE_CONNECTING_TO_LOBBY, // Once a player clicks "Join", we try to join a steam lobby
	NETWORK_BATTLE_STATE_JOINING, // When we connect to a steam lobby successfully, we create a GGPO session and start updating battle state
};


struct NetworkBattle
{
	// Steam data
	SteamAPICall_t lobby_create_call;
	uint64_t lobby_id;

	// Battle data
	GGPOSession *ggpo_session;
	GGPOPlayer ggpo_players[2];
	GGPOPlayerHandle ggpo_player_handles[2];

	// State data
	enum NetworkBattleState state;

	// UI data
	bool host_pressed;
	bool join_pressed;
};


// clicked Host on the menu
void network_battle_init(struct Game *game);
// joined through Steam overlay
void network_battle_state_join(struct Game *game, uint64_t lobby_id);

void network_battle_term(struct Game *game);
bool network_battle_update(struct Game *game, struct GameUpdateContext const *ctx);
void network_battle_steam_callback(struct Game *game, struct GameUpdateContext const *ctx, int callback_type, void *callback_data, int callback_datasize);
void network_battle_render(struct Game *game);
