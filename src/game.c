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

static void game_steam_callback(struct Game *game, int callback_type, void *callback_data, int callback_datasize)
{
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
		// Check for dispatching API call results
		// if (callback.m_iCallback == SteamAPICallCompleted_t::k_iCallback)
		// {
		// 	SteamAPICallCompleted_t* pCallCompleted = (SteamAPICallCompleted_t*)callback.
		// 		void* pTmpCallResult = malloc(pCallback->m_cubParam);
		// 	bool bFailed;
		// 	if (SteamAPI_ManualDispatch_GetAPICallResult(hSteamPipe, pCallCompleted->m_hAsyncCall, pTmpCallResult, pCallback->m_cubParam, pCallback->m_iCallback, &bFailed))
		// 	{
		// 		// Dispatch the call result to the registered handler(s) for the
		// 		// call identified by pCallCompleted->m_hAsyncCall
		// 	}
		// 	free(pTmpCallResult);
		// }
		// else
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
