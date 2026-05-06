#include "game.h"

#include "steam_api_c.h"

void game_first_init(struct Game *game)
{
	// We need to call steam init before any window or graphics context is created.
	SteamErrMsg errMsg = { 0 };
	if (SteamAPI_InitFlat(&errMsg) != k_ESteamAPIInitResult_OK) {
		fprintf(stderr, "Failed to init Steam: %s.\n", errMsg);
		assert(false);
		return;
	}
	// Enable relay for P2P
	SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess(SteamAPI_SteamNetworkingUtils_SteamAPI());
	// C api need a manual dispatch for callbacks
	SteamAPI_ManualDispatch_Init();

	uint64_t user_id = SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser());
	fprintf(stdout, "Init steam success: user id %llu.\n", user_id);
}

void game_init(struct Game *game)
{
	STATE_INIT_FUNCTIONS[game->current_state](&game->state_data, game);
}

void network_battle_state_join(struct NetworkBattleData *data, uint64_steamid lobby_id);
static void game_steam_callback(struct Game *game, int callback_type, void *callback_data, int callback_datasize)
{
	if (callback_type == k_iSteamUtilsSteamAPICallCompletedCallback) {
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

				STATE_TERM_FUNCTIONS[game->current_state](&game->state_data);
				game->current_state = GAME_STATE_NETWORK_BATTLE;
				game->requested_state = GAME_STATE_NETWORK_BATTLE;
				STATE_INIT_FUNCTIONS[game->current_state](&game->state_data, game);
				network_battle_state_join((struct NetworkBattleData*)game->state_data, lobby_enter.m_ulSteamIDLobby);

				game->lobby_join_request = 0;
			}
		} else {
			StateCallbackFn state_callback = STATE_STEAM_CALLBACK_FUNCTIONS[game->current_state];
			if (state_callback) {
				state_callback(&game->state_data, NULL, callback_type, callback_data, callback_datasize);
			}
		}

	} else if (callback_type == k_iSteamMatchmakingLobbyDataUpdateCallback) {

		// assert(callback_datasize >= sizeof(LobbyDataUpdate_t)); // wrong?
		LobbyDataUpdate_t *update = (LobbyDataUpdate_t*)callback_data;
		if (update->m_bSuccess != 0 && update->m_ulSteamIDLobby == game->lobby_id) {
			fprintf(stdout, "Data update for lobby %llu member %llu.\n", game->lobby_id, update->m_ulSteamIDMember);
		} else {
			// error happened we are no longer in lobby
			game->lobby_id = 0;
		}

	} else if (callback_type == k_iSteamFriendsGameLobbyJoinRequestedCallback) {

		assert(callback_datasize == sizeof(GameLobbyJoinRequested_t));
		GameLobbyJoinRequested_t *join_request = (GameLobbyJoinRequested_t*)callback_data;
		if (game->lobby_join_request == 0) {
			fprintf(stdout, "Request to join lobby %llu.\n", join_request->m_steamIDLobby);
			game->lobby_join_request = SteamAPI_ISteamMatchmaking_JoinLobby(SteamAPI_SteamMatchmaking(), join_request->m_steamIDLobby);
		}

	} else {
		StateCallbackFn state_callback = STATE_STEAM_CALLBACK_FUNCTIONS[game->current_state];
		if (state_callback) {
			state_callback(&game->state_data, NULL, callback_type, callback_data, callback_datasize);
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
		/**
		// Check for dispatching API call results
		if (callback.m_iCallback == k_SteamAPICallCompleted_t::k_iCallback)
		{
		 	SteamAPICallCompleted_t* pCallCompleted = (SteamAPICallCompleted_t*)callback.
			void* pTmpCallResult = malloc(pCallback->m_cubParam);
			bool bFailed;
		 	if (SteamAPI_ManualDispatch_GetAPICallResult(hSteamPipe, pCallCompleted->m_hAsyncCall, pTmpCallResult, pCallback->m_cubParam, pCallback->m_iCallback, &bFailed))
		 	{
		 		// Dispatch the call result to the registered handler(s) for the
		 		// call identified by pCallCompleted->m_hAsyncCall
		 	}
		 	free(pTmpCallResult);
		}
		else
		**/
		{
			// Look at callback.m_iCallback to see what kind of callback it is,
			// and dispatch to appropriate handler(s)
			game_steam_callback(game, callback.m_iCallback, callback.m_pubParam, callback.m_cubParam);
		}
		SteamAPI_ManualDispatch_FreeLastCallback(hSteamPipe);
	}

	if (game->requested_state != game->current_state) {
		enum GameState prev_state = game->current_state;
		enum GameState next_state = game->requested_state;
		game->current_state = game->requested_state;

		STATE_TERM_FUNCTIONS[prev_state](&game->state_data);
		STATE_INIT_FUNCTIONS[next_state](&game->state_data, game);
	}

	struct GameUpdateResult update_res = STATE_UPDATE_FUNCTIONS[game->current_state](&game->state_data, ctx);
	if (update_res.action == GAME_UPDATE_ACTION_TRANSITION) {
		game->requested_state = update_res.transition_state;
	}
}

void game_render(struct Game *game)
{
	STATE_RENDER_FUNCTIONS[game->current_state](&game->state_data);
}
