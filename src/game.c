#include "game.h"

#include "steam_api_c.h"

void game_first_init(struct Game *game)
{
	// We need to call steam init before any window or graphics context is created.
	SteamErrMsg errMsg = { 0 };
	if (SteamAPI_InitFlat(&errMsg) != k_ESteamAPIInitResult_OK) {
		fprintf(stderr, "Failed to init Steam: %s.\n", errMsg);
		ASSERT(false);
		return;
	}
	// Enable relay for P2P
	SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(SteamAPI_SteamNetworkingUtils_SteamAPI());
	// C api need a manual dispatch for callbacks
	SteamAPI_ManualDispatch_Init();

	game->user_id = SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser());
	fprintf(stdout, "Init steam success: user id %llu.\n", game->user_id);
}

void game_init(struct Game *game)
{
	game->current_state = GAME_STATE_MAIN_MENU;
	mainmenu_init(&game->mainmenu);
}

static void game_join_steam_lobby(struct Game *game, uint64_t lobby_id)
{
	switch (game->current_state) {
	case GAME_STATE_MAIN_MENU: {
		// clean up?
		break;
	}
	case GAME_STATE_LOCAL_BATTLE: {
		// clean up?
		break;
	}
	case GAME_STATE_NETWORK_BATTLE: {
		// clean up?
		break;
	}
	}

	game->lobby_join_request = 0;

	game->current_state = GAME_STATE_NETWORK_BATTLE;
	network_battle_on_lobby_joined(game, lobby_id);
}

static void game_steam_callback(struct Game *game, int callback_type, void *callback_data, int callback_datasize)
{
	bool handled = false;
	if (callback_type == k_iSteamUtilsSteamAPICallCompletedCallback) {
		fprintf(stdout, "[steam]  steam api call completed.\n");

		SteamAPICallCompleted_t* pCallCompleted = (SteamAPICallCompleted_t*)callback_data;
		// Lobby Entered
		if (pCallCompleted->m_hAsyncCall == game->lobby_join_request) {
			LobbyEnter_t lobby_enter = {0};
			bool bFailed = false;
			if (SteamAPI_ManualDispatch_GetAPICallResult(SteamAPI_GetHSteamPipe(),
								     pCallCompleted->m_hAsyncCall,
								     &lobby_enter,
								     pCallCompleted->m_cubParam,
								     pCallCompleted->m_iCallback,
								     &bFailed)) {

				uint64_t lobby_id = lobby_enter.m_ulSteamIDLobby;
				int num_players = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(SteamAPI_SteamMatchmaking(), lobby_id);
				if (num_players == 2) {
					uint64_t p0_id = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(SteamAPI_SteamMatchmaking(), lobby_id, 0);
					uint64_t p1_id = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(SteamAPI_SteamMatchmaking(), lobby_id, 1);
					fprintf(stdout, "[steam] P0 %llu P1 %llu\n", p0_id, p1_id);
				}

				if (lobby_enter.m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess) {
					// Lobby was joined
					fprintf(stdout, "[steam] joined lobby %llu.\n", lobby_id);
					game->lobby_id = lobby_enter.m_ulSteamIDLobby;
				} else {
					fprintf(stderr, "[steam] failed to joined lobby %llu.\n", lobby_id);
				}

				game_join_steam_lobby(game, lobby_id);
			}
			handled = true;
		}

	} else if (callback_type == k_iSteamMatchmakingLobbyDataUpdateCallback) {

		fprintf(stdout, "[steam]  lobby data update.\n");
		// ASSERT(callback_datasize >= sizeof(LobbyDataUpdate_t)); // wrong?
		LobbyDataUpdate_t *update = (LobbyDataUpdate_t*)callback_data;
		if (update->m_bSuccess != 0 && update->m_ulSteamIDLobby == game->lobby_id) {
			fprintf(stdout, "Data update for lobby %llu member %llu.\n", game->lobby_id, update->m_ulSteamIDMember);
		} else {
			// error happened we are no longer in lobby
			game->lobby_id = 0;
		}
		handled = true;

	} else if (callback_type == k_iSteamFriendsGameLobbyJoinRequestedCallback) {

		fprintf(stdout, "[steam]  friend lobby join requested.\n");
		ASSERT(callback_datasize == sizeof(GameLobbyJoinRequested_t));
		GameLobbyJoinRequested_t *join_request = (GameLobbyJoinRequested_t*)callback_data;
		if (game->lobby_join_request == 0) {
			fprintf(stdout, "Request to join lobby %llu.\n", join_request->m_steamIDLobby);
			game->lobby_join_request = SteamAPI_ISteamMatchmaking_JoinLobby(SteamAPI_SteamMatchmaking(), join_request->m_steamIDLobby);
		}
		handled = true;

	} else if (callback_type == k_iSteamMatchmakingLobbyChatUpdateCallback) {
		fprintf(stdout, "[steam]  lobby chat update.\n");
	} else if (callback_type == k_iSteamMatchmakingLobbyEnterCallback) {
		fprintf(stdout, "[steam]  lobby enter.\n");
	}

	if (!handled) {
		switch (game->current_state) {
		case GAME_STATE_MAIN_MENU: {
			break;
		}
		case GAME_STATE_LOCAL_BATTLE: {
			break;
		}
		case GAME_STATE_NETWORK_BATTLE: {
			network_battle_steam_callback(game, NULL, callback_type, callback_data, callback_datasize);
			break;
		}
		}
	}
}

void game_update(struct Game *game, struct GameUpdateContext const* ctx)
{
	debug_draw_reset();

	if (ImGui_Begin("Debug", NULL, 0)) {
		ImGui_Text("current time: %llu", ctx->current_time);
		ImGui_Text("frame: %llu", ctx->f);
		ImGui_Text("previous frame time: %llu", ctx->previous_frame_time);
	}
	ImGui_End();

	HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();
	SteamAPI_ManualDispatch_RunFrame(hSteamPipe);
	CallbackMsg_t callback;
	while (SteamAPI_ManualDispatch_GetNextCallback(hSteamPipe, &callback)) {
		game_steam_callback(game, callback.m_iCallback, callback.m_pubParam, callback.m_cubParam);
		SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
	}

	bool rerun = false;
	do {
	switch (game->current_state) {
	case GAME_STATE_MAIN_MENU: {
		rerun = mainmenu_update(game, ctx);
		break;
	}
	case GAME_STATE_LOCAL_BATTLE: {
		rerun = local_battle_update(game, ctx);
		break;
	}
	case GAME_STATE_NETWORK_BATTLE: {
		rerun = network_battle_update(game, ctx);
		break;
	}
	}
	} while(rerun);
}

void game_render(struct Game *game)
{
	switch (game->current_state) {
	case GAME_STATE_MAIN_MENU: {
		mainmenu_render(&game->mainmenu);
		break;
	}
	case GAME_STATE_LOCAL_BATTLE: {
		local_battle_render(game);
		break;
	}
	case GAME_STATE_NETWORK_BATTLE: {
		network_battle_render(game);
		break;
	}
	}
}
