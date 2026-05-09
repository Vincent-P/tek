#include "game_network_battle.h"

#include "steam_api_c.h"


// ggpo callbacks
struct Game *ggpo_game_global_state = NULL;


void tek_check_error(GGPOErrorCode err)
{
	assert(err == 0);
}

/*
 * begin_game callback - This callback has been deprecated.  You must
 * implement it, but should ignore the 'game' parameter.
 */
bool tek_begin_game(const char *game)
{
	return true;
}

/*
 * save_game_state - The client should allocate a buffer, copy the
 * entire contents of the current game state into it, and copy the
 * length into the *len parameter.  Optionally, the client can compute
 * a checksum of the data and store it in the *checksum argument.
 */
bool tek_save_game_state(unsigned char **out_buffer, int *out_len, int *out_checksum, int frame)
{
	int length = sizeof(struct BattleState);
	void *gs_copy = malloc(length);
	memcpy(gs_copy, &ggpo_game_global_state->simulation.battle_context.battle_state, length);

	*out_buffer = gs_copy;
	*out_len = length;
	*out_checksum = 0;

	return true;
}

/*
 * load_game_state - GGPO.net will call this function at the beginning
 * of a rollback.  The buffer and len parameters contain a previously
 * saved state returned from the save_game_state function.  The client
 * should make the current game state match the state contained in the
 * buffer.
 */
bool tek_load_game_state(unsigned char *buffer, int len)
{
	int length = sizeof(struct BattleState);
	if (len == length) {
		memcpy(&ggpo_game_global_state->simulation.battle_context.battle_state, buffer, length);
		return true;
	}
	return false;
}

/*
 * log_game_state - Used in diagnostic testing.  The client should use
 * the ggpo_log function to write the contents of the specified save
 * state in a human readible form.
 */
bool tek_log_game_state(char *filename, unsigned char *buffer, int len)
{
	int length = sizeof(struct BattleState);
	if (len == length) {
		struct BattleState *gs = (struct BattleState *)buffer;
		ggpo_log(ggpo_game_global_state->network_battle.ggpo_session, "tek frame: %u", gs->frame_number);
		return true;
	}
	return false;
}

/*
 * free_buffer - Frees a game state allocated in save_game_state.  You
 * should deallocate the memory contained in the buffer.
 */
void tek_free_buffer(void *buffer)
{
	free(buffer);
}

/*
 * advance_frame - Called during a rollback.  You should advance your game
 * state by exactly one frame.  Before each frame, call ggpo_synchronize_input
 * to retrieve the inputs you should use for that frame.  After each frame,
 * you should call ggpo_advance_frame to notify GGPO.net that you're
 * finished.
 *
 * The flags parameter is reserved.  It can safely be ignored at this time.
 */
bool tek_advance_frame(int flags)
{
	GGPOSession* session = ggpo_game_global_state->network_battle.ggpo_session;
	struct BattleContext *battle_ctx = &ggpo_game_global_state->simulation.battle_context;

	struct BattleInputs network_inputs = {0};
	int disconnect_flags = 0;
	GGPOErrorCode err;
	err = ggpo_synchronize_input(session, &network_inputs, sizeof(network_inputs), &disconnect_flags);
	tek_check_error(err);
	enum BattleFrameResult battle_result = battle_simulate_frame(battle_ctx, network_inputs);
	err = ggpo_advance_frame(session);
	tek_check_error(err);

	return true;
}

/*
 * on_event - Notification that something has happened.  See the GGPOEventCode
 * structure above for more information.
 */
bool tek_on_event(GGPOEvent *info)
{
	if (info->code == GGPO_EVENTCODE_CONNECTED_TO_PEER) {
		printf("NETWORK_BATTLE: [ggpo] connected to peer\n");
	} else if (info->code == GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER) {
		printf("NETWORK_BATTLE: [ggpo] synchronizing with peer\n");
	} else if (info->code == GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER) {
		printf("NETWORK_BATTLE: [ggpo] synchronized with peer\n");
	} else if (info->code == GGPO_EVENTCODE_RUNNING) {
		printf("NETWORK_BATTLE: [ggpo] running\n");
	} else if (info->code == GGPO_EVENTCODE_DISCONNECTED_FROM_PEER) {
		printf("NETWORK_BATTLE: [ggpo] disconnected from peer\n");
	} else if (info->code == GGPO_EVENTCODE_TIMESYNC) {
		printf("NETWORK_BATTLE: [ggpo] time sync\n");
	} else if (info->code == GGPO_EVENTCODE_CONNECTION_INTERRUPTED) {
		printf("NETWORK_BATTLE: [ggpo] connection interupted\n");
	} else if (info->code == GGPO_EVENTCODE_CONNECTION_RESUMED) {
		printf("NETWORK_BATTLE: [ggpo] connection resumed\n");
	}

	return true;
}



// steam callback
void network_battle_state_host(struct Game *game);
void network_battle_steam_callback(struct Game *game, struct GameUpdateContext const *ctx, int callback_type, void *callback_data, int callback_datasize)
{
	struct NetworkBattle *data = &game->network_battle;

	if (callback_type == k_iSteamUtilsSteamAPICallCompletedCallback) {
		SteamAPICallCompleted_t* pCallCompleted = (SteamAPICallCompleted_t*)callback_data;

		if (data->state == NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_CREATION) {
			// we are waiting for a lobby to be created
			if (pCallCompleted->m_hAsyncCall == data->lobby_create_call) {
				LobbyCreated_t lobby_created_result = {0};
				bool bFailed = false;
				if (SteamAPI_ManualDispatch_GetAPICallResult(SteamAPI_GetHSteamPipe(),
									     pCallCompleted->m_hAsyncCall,
									     &lobby_created_result,
									     pCallCompleted->m_cubParam,
									     pCallCompleted->m_iCallback,
									     &bFailed)) {
					// Lobby was created
					data->lobby_id = lobby_created_result.m_ulSteamIDLobby;
					if (data->lobby_id != 0) {
						fprintf(stdout, "[steam] created lobby %llu.\n", data->lobby_id);

						int num_players = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(SteamAPI_SteamMatchmaking(), data->lobby_id);
						if (num_players == 2) {
							uint64_t p0_id = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(SteamAPI_SteamMatchmaking(), data->lobby_id, 0);
							uint64_t p1_id = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(SteamAPI_SteamMatchmaking(), data->lobby_id, 1);
						}

						SteamAPI_ISteamMatchmaking_SetLobbyData(SteamAPI_SteamMatchmaking(), data->lobby_id, "name", "tek");

						data->state = NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_PLAYERS;
					} else {
						data->state = NETWORK_BATTLE_STATE_MAIN_MENU;
						data->lobby_create_call = 0;
					}
				}
			}
		}
	} else if (callback_type == k_iSteamMatchmakingLobbyChatUpdateCallback) {
		if (data->state == NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_PLAYERS) {

			bool const is_host = data->lobby_create_call != 0;
			if (is_host) {
				// LobbyChatUpdate update is called when a user joins or leaves a lobby.
				LobbyChatUpdate_t *update = (LobbyChatUpdate_t*)callback_data;
				if (update->m_ulSteamIDLobby == data->lobby_id && update->m_rgfChatMemberStateChange == k_EChatMemberStateChangeEntered) {
					if (update->m_ulSteamIDUserChanged != game->user_id) {
						data->player_steam_ids[0] = game->user_id;
						data->player_steam_ids[1] = update->m_ulSteamIDUserChanged;

						data->state = NETWORK_BATTLE_STATE_PLAY;
						network_battle_state_host(game);
					}
				}
			}
		}
	}
}


// state logic
void network_battle_state_host(struct Game *game)
{
	struct Simulation *simulation = &game->simulation;
	struct NetworkBattle *data = &game->network_battle;
	data->state = NETWORK_BATTLE_STATE_PLAY;

	ggpo_game_global_state = game;
	memset(data->ggpo_players, 0, ARRAY_LENGTH(data->ggpo_players) * sizeof(GGPOPlayer));
	data->ggpo_players[0].size = sizeof(GGPOPlayer);
	data->ggpo_players[0].type = GGPO_PLAYERTYPE_LOCAL;
	data->ggpo_players[0].player_num = 1;
	data->ggpo_players[1].size = sizeof(GGPOPlayer);
	data->ggpo_players[1].type = GGPO_PLAYERTYPE_REMOTE;
	data->ggpo_players[1].player_num = 2;
	data->ggpo_players[1].u.steam_remote.steam_id = data->player_steam_ids[1];
	GGPOSessionCallbacks session_callbacks = {0};
	session_callbacks.begin_game = tek_begin_game;
	session_callbacks.save_game_state = tek_save_game_state;
	session_callbacks.load_game_state = tek_load_game_state;
	session_callbacks.log_game_state = tek_log_game_state;
	session_callbacks.free_buffer = tek_free_buffer;
	session_callbacks.advance_frame = tek_advance_frame;
	session_callbacks.on_event = tek_on_event;
	GGPOErrorCode err = ggpo_start_session(&data->ggpo_session, &session_callbacks, "tek", 2, sizeof(struct BattleInput), 0);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[0], &data->ggpo_player_handles[0]);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[1], &data->ggpo_player_handles[1]);
	tek_check_error(err);

	// Init battle
	memset(&simulation->battle_context, 0, sizeof(simulation->battle_context));
	simulation->battle_context.assets = game->assets;
	simulation->battle_context.renderer = game->renderer;
	simulation->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&simulation->battle_context);
}

void network_battle_on_lobby_joined(struct Game *game, uint64_t lobby_id)
{
	struct NetworkBattle *data = &game->network_battle;
	struct Simulation *simulation = &game->simulation;

	data->state = NETWORK_BATTLE_STATE_PLAY;

	fprintf(stdout, "[steam] joined lobby %llu.\n", lobby_id);
	int num_players = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(SteamAPI_SteamMatchmaking(), lobby_id);

	uint64_t remote_player_id = 0;
	if (num_players == 2) {
		uint64_t p0_id = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(SteamAPI_SteamMatchmaking(), lobby_id, 0);
		uint64_t p1_id = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(SteamAPI_SteamMatchmaking(), lobby_id, 1);
		fprintf(stdout, "[steam] P0 %llu P1 %llu\n", p0_id, p1_id);

		if (p0_id != game->user_id)
			remote_player_id = p0_id;
		if (p1_id != game->user_id)
			remote_player_id = p1_id;
	}

	ggpo_game_global_state = game;
	data->ggpo_players[0].size = sizeof(GGPOPlayer);
	data->ggpo_players[0].type = GGPO_PLAYERTYPE_REMOTE;
	data->ggpo_players[0].player_num = 1;
	data->ggpo_players[0].u.steam_remote.steam_id = remote_player_id;
	data->ggpo_players[1].size = sizeof(GGPOPlayer);
	data->ggpo_players[1].type = GGPO_PLAYERTYPE_LOCAL;
	data->ggpo_players[1].player_num = 2;
	GGPOSessionCallbacks session_callbacks = {0};
	session_callbacks.begin_game = tek_begin_game;
	session_callbacks.save_game_state = tek_save_game_state;
	session_callbacks.load_game_state = tek_load_game_state;
	session_callbacks.log_game_state = tek_log_game_state;
	session_callbacks.free_buffer = tek_free_buffer;
	session_callbacks.advance_frame = tek_advance_frame;
	session_callbacks.on_event = tek_on_event;
	GGPOErrorCode err = ggpo_start_session(&data->ggpo_session, &session_callbacks, "tek", 2, sizeof(struct BattleInput), 0);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[0], &data->ggpo_player_handles[0]);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[1], &data->ggpo_player_handles[1]);
	tek_check_error(err);

	// Init battle
	memset(&simulation->battle_context, 0, sizeof(simulation->battle_context));
	simulation->battle_context.assets = game->assets;
	simulation->battle_context.renderer = game->renderer;
	simulation->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&simulation->battle_context);
}

void network_battle_init(struct Game *game)
{
	printf("NETWORK_BATTLE: Init\n");
	ggpo_log(NULL, "test");
}

void network_battle_term(struct Game *game)
{
	printf("NETWORK_BATTLE: Term\n");
	struct NetworkBattle *data = &game->network_battle;
	struct Simulation *simulation = &game->simulation;

	battle_state_term(&simulation->battle_context);

	GGPOErrorCode err = ggpo_close_session(data->ggpo_session);
	tek_check_error(err);
	data->ggpo_session = NULL;
	ggpo_game_global_state = NULL;
}

bool network_battle_update(struct Game *game, struct GameUpdateContext const *ctx)
{
	struct NetworkBattle *data = &game->network_battle;
	struct Simulation *simulation = &game->simulation;
	bool result = false;
	switch (data->state) {
	case NETWORK_BATTLE_STATE_MAIN_MENU: {
		if (data->host_pressed) {
			data->state = NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_CREATION;
			data->lobby_create_call = SteamAPI_ISteamMatchmaking_CreateLobby(SteamAPI_SteamMatchmaking(), k_ELobbyTypeFriendsOnly, 2);
			data->host_pressed = false;
		}
		if (data->join_pressed) {
			// network_battle_state_join(data);
			data->join_pressed = false;
		}
		break;
	}

	case NETWORK_BATTLE_STATE_PLAY: {


		GGPOErrorCode err = 0;
		err = ggpo_idle(data->ggpo_session, 5);
		tek_check_error(err);


		if (ImGui_Begin("GGPO stats", NULL, 0)) {
			GGPONetworkStats stats = {0};
			ggpo_get_network_stats(data->ggpo_session, data->ggpo_player_handles[0], &stats);

			ImGui_Text("P1:");
			ImGui_Text("network.send_queue_len: %d", stats.network.send_queue_len);
			ImGui_Text("network.recv_queue_len: %d", stats.network.recv_queue_len);
			ImGui_Text("network.ping: %d", stats.network.ping);
			ImGui_Text("network.kbps_sent: %d", stats.network.kbps_sent);
			ImGui_Text("timesync.local_frames_behind: %d", stats.timesync.local_frames_behind);
			ImGui_Text("timesync.remote_frames_behind: %d", stats.timesync.remote_frames_behind);

			ggpo_get_network_stats(data->ggpo_session, data->ggpo_player_handles[1], &stats);
			ImGui_Text("P2:");
			ImGui_Text("network.send_queue_len: %d", stats.network.send_queue_len);
			ImGui_Text("network.recv_queue_len: %d", stats.network.recv_queue_len);
			ImGui_Text("network.ping: %d", stats.network.ping);
			ImGui_Text("network.kbps_sent: %d", stats.network.kbps_sent);
			ImGui_Text("timesync.local_frames_behind: %d", stats.timesync.local_frames_behind);
			ImGui_Text("timesync.remote_frames_behind: %d", stats.timesync.remote_frames_behind);
		}
		ImGui_End();

		// Playing. Read inputs and simulate battle.
		bool const is_host = data->lobby_create_call != 0;
		simulation->accumulator += ctx->previous_frame_time;
		const uint64_t dt = 16;
		while (simulation->accumulator >= dt) {
			TracyCZoneN(f, "Battle Frame", true);
			struct BattleInputs battle_inputs = battle_read_input(game->inputs);

			if (is_host) {
				// Send player1 inputs to network
				err = ggpo_add_local_input(data->ggpo_session, data->ggpo_player_handles[0], &battle_inputs.player1, sizeof(struct BattleInput));
			} else {
				// Send player2 inputs to network
				err = ggpo_add_local_input(data->ggpo_session, data->ggpo_player_handles[1], &battle_inputs.player1, sizeof(struct BattleInput));
			}
			if (GGPO_SUCCEEDED(err)) {
				tek_check_error(err);
				// Receive both inputs from network
				struct BattleInputs network_inputs = {0};
				int disconnect_flags = 0;
				err = ggpo_synchronize_input(data->ggpo_session, &network_inputs, sizeof(network_inputs), &disconnect_flags);
				if (GGPO_SUCCEEDED(err)) {
					tek_check_error(err);
					enum BattleFrameResult battle_result = battle_simulate_frame(&simulation->battle_context, network_inputs);
					TracyCZoneEnd(f);

					err = ggpo_advance_frame(data->ggpo_session);
					tek_check_error(err);

					if (battle_result != BATTLE_FRAME_RESULT_CONTINUE || disconnect_flags != 0) {
						network_battle_term(game);
						network_battle_init(game);
						data->state = NETWORK_BATTLE_STATE_MAIN_MENU;
						break;
					}
				}
			}

			simulation->t += dt;
			simulation->accumulator -= dt;
		}
		break;
	}
	}

	return result;
}

void network_battle_render(struct Game *game)
{
	struct NetworkBattle *data = &game->network_battle;
	struct Simulation *simulation = &game->simulation;

	if (data->state == NETWORK_BATTLE_STATE_PLAY) {
		battle_render(&simulation->battle_context);
	}

	float p1_hp_filled = simulation->battle_context.battle_state.p1_entity.tek.hp * 0.01f;
	float p2_hp_filled = simulation->battle_context.battle_state.p2_entity.tek.hp * 0.01f;
	int rounds_first_to = simulation->battle_context.battle_non_state.rounds_first_to;
	int rounds_p1_won = simulation->battle_context.battle_non_state.rounds_p1_won;
	int rounds_p2_won = simulation->battle_context.battle_non_state.rounds_p2_won;

	CLAY({
			.id = CLAY_ID("OuterContainer"),
			.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
			.backgroundColor = {0,0,0,0}
		}) {



		if (data->state == NETWORK_BATTLE_STATE_MAIN_MENU) {
			CLAY({.id = CLAY_ID("MAIN_MENU_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
				CLAY({
						.id = CLAY_ID("Floating"),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIT(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.floating = { .offset = {0, 16}, .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_CENTER } },
						.backgroundColor = {0, 0, 0, 128}
					}) {
					ui_button("Host", &data->host_pressed);
					ui_button("Join", &data->join_pressed);
				}
			}
		} else if (data->state == NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_CREATION) {
			CLAY({.id = CLAY_ID("MAIN_MENU_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
				CLAY({
						.id = CLAY_ID("Floating"),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIT(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.floating = { .offset = {0, 16}, .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_CENTER } },
						.backgroundColor = {0, 0, 0, 128}
					}) {
					CLAY_TEXT(CLAY_STRING("Waiting for lobby creation"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255}}));
				}
			}
		} else if (data->state == NETWORK_BATTLE_STATE_WAITING_FOR_LOBBY_PLAYERS) {
			CLAY({.id = CLAY_ID("MAIN_MENU_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
				CLAY({
						.id = CLAY_ID("Floating"),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIT(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.floating = { .offset = {0, 16}, .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_CENTER } },
						.backgroundColor = {0, 0, 0, 128}
					}) {
					CLAY_TEXT(CLAY_STRING("Waiting for players..."), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255}}));



					// 64 bits total
					union SteamID_t
					{
						struct SteamIDComponent_t
						{
#ifdef VALVE_BIG_ENDIAN
							uint64_t			m_EUniverse : 8;	// universe this account belongs to
							uint64_t		m_EAccountType : 4;			// type of account - can't show as EAccountType, due to signed / unsigned difference
							uint64_t		m_unAccountInstance : 20;	// dynamic instance ID
							uint64_t				m_unAccountID : 32;			// unique account identifier
#else
							uint64_t				m_unAccountID : 32;			// unique account identifier
							uint64_t		m_unAccountInstance : 20;	// dynamic instance ID
							uint64_t		m_EAccountType : 4;			// type of account - can't show as EAccountType, due to signed / unsigned difference
							uint64_t			m_EUniverse : 8;	// universe this account belongs to
#endif
						} m_comp;

						uint64_t m_unAll64Bits;
					};

					union SteamID_t lobby_id = {0};
					lobby_id.m_unAll64Bits = data->lobby_id;

					static char buf[256] = {0};
					Clay_String s = {0};
					s.length = snprintf(buf, sizeof(buf), "%lld", lobby_id.m_comp.m_unAccountID);
					s.chars = buf;
					CLAY_TEXT(s, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255}}));

					static char buf2[256] = {0};
					s.length = snprintf(buf2, sizeof(buf2), "%llu", lobby_id.m_comp.m_unAccountID);
					s.chars = buf2;
					CLAY_TEXT(s, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255}}));
				}
			}
		} else if (data->state == NETWORK_BATTLE_STATE_PLAY) {
			CLAY({.id = CLAY_ID("HeaderBar"), .layout = { .sizing = {.width = CLAY_SIZING_GROW(0)}, .childGap = 16}}) {

				CLAY({
						.id = CLAY_IDI("PlayerBar", 1),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {.width = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.backgroundColor = {0, 0, 0, 64}
					}) {

					CLAY({
							.id = CLAY_IDI("HealthBar", 1),
							.layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) }},
							.backgroundColor = {0, 0, 255, 255}
						}) {

						CLAY({.id = CLAY_IDI("EmptyHealth", 1),
								.layout = { .sizing = { .width = CLAY_SIZING_PERCENT(1.0f - p1_hp_filled), .height = CLAY_SIZING_GROW(0) }},
								.backgroundColor = {0, 0, 0, 255},
							});
						CLAY({.id = CLAY_IDI("FilledHealth", 1),
								.layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }},
								.backgroundColor = {240, 240, 240, 255},
							});
					}
					CLAY({.id = CLAY_IDI("PlayerInfo", 1), .layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {
						CLAY({.id = CLAY_IDI("PlayerName", 1)}) {
							CLAY_TEXT(CLAY_STRING("SuperBob"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255}}));
					        }
						CLAY({.id = CLAY_IDI("PlayerFill", 1), .layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) { }
						CLAY({.id = CLAY_IDI("PlayerRounds", 1)}) {
							for (int i = 1; i <= rounds_first_to; ++i) {
								if (i <= rounds_p1_won) {
									CLAY_TEXT(CLAY_STRING("X"), CLAY_TEXT_CONFIG({.fontSize = 32, .textColor = {255, 255, 255, 255}}));
								} else {
									CLAY_TEXT(CLAY_STRING("O"), CLAY_TEXT_CONFIG({.fontSize = 32, .textColor = {255, 255, 255, 255}}));
								}
							}
					        }
					}
				}

				CLAY_TEXT(CLAY_STRING("59"), CLAY_TEXT_CONFIG({ .fontSize = 50, .textColor = {255, 255, 255, 255} }));

				CLAY({
						.id = CLAY_IDI("PlayerBar", 2),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {.width = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = {.x = CLAY_ALIGN_X_RIGHT} },
						.backgroundColor = {0, 0, 0, 64}
					}) {

					CLAY({
							.id = CLAY_IDI("HealthBar", 2),
							.layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) }},
							.backgroundColor = {0, 0, 255, 255}
						}) {

						CLAY({.id = CLAY_IDI("FilledHealth", 2),
								.layout = { .sizing = { .width = CLAY_SIZING_PERCENT(p2_hp_filled), .height = CLAY_SIZING_GROW(0) }},
								.backgroundColor = {240, 240, 240, 255},
							});
						CLAY({.id = CLAY_IDI("EmptyHealth", 2),
								.layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }},
								.backgroundColor = {0, 0, 0, 255},
							});
					}

					CLAY({.id = CLAY_IDI("PlayerInfo", 2), .layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) {
						CLAY({.id = CLAY_IDI("PlayerRounds", 2)}) {
							for (int i = 1; i <= rounds_first_to; ++i) {
								if (i <= rounds_p2_won) {
									CLAY_TEXT(CLAY_STRING("X"), CLAY_TEXT_CONFIG({.fontSize = 32, .textColor = {255, 255, 255, 255}}));
								} else {
									CLAY_TEXT(CLAY_STRING("O"), CLAY_TEXT_CONFIG({.fontSize = 32, .textColor = {255, 255, 255, 255}}));
								}
							}
					        }
						CLAY({.id = CLAY_IDI("PlayerFill", 2), .layout = {.sizing = {CLAY_SIZING_GROW(0)}}}) { }
						CLAY({.id = CLAY_IDI("PlayerName", 2)}) {
							CLAY_TEXT(CLAY_STRING("Seb"), CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {255, 255, 255, 255}}));
					        }
					}

				}


			}

			CLAY({.id = CLAY_ID("HUD"), .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = {.width = CLAY_SIZING_GROW(0)} }}) {

				CLAY({
						.id = CLAY_IDI("PlayerInput", 1),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
					}) {
					struct BattleState* battle_state = &simulation->battle_context.battle_state;
					struct TekPlayerComponent const *p1 = &battle_state->p1_entity.tek;
					#if 0
					uint32_t input_last_frame = battle_state->frame_number;
					for (uint32_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
						uint32_t input_index = (p1->current_input_index + (INPUT_BUFFER_SIZE - i)) % INPUT_BUFFER_SIZE;
						enum BattleInputBits input = p1->input_buffer[input_index];
						uint32_t input_duration = input_last_frame - p1->input_buffer_frame_start[input_index];
						input_last_frame = p1->input_buffer_frame_start[input_index];

						char label[64] = {0};
						uint32_t label_length = sprintf(label, "%s %s - %u", _get_motion_label(input), _get_action_label(input), input_duration);

						Clay_String label_string = (Clay_String){
							.isStaticallyAllocated = false,
							.length = label_length,
							.chars = ui_string(label, label_length),
						};
						CLAY_TEXT(label_string, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {0, 0, 0, 255}}));
					}
					#endif
				}

				CLAY({.id = CLAY_ID("Empty"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {}

				CLAY({
						.id = CLAY_IDI("PlayerInput", 2),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = {.x = CLAY_ALIGN_X_RIGHT} },
					}) {
					struct BattleState* battle_state = &simulation->battle_context.battle_state;
					struct TekPlayerComponent const *p2 = &battle_state->p2_entity.tek;

					#if 0
					uint32_t input_last_frame = battle_state->frame_number;
					for (uint32_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
						uint32_t input_index = (p2->current_input_index + (INPUT_BUFFER_SIZE - i)) % INPUT_BUFFER_SIZE;
						enum BattleInputBits input = p2->input_buffer[input_index];
						uint32_t input_duration = input_last_frame - p2->input_buffer_frame_start[input_index];
						input_last_frame = p2->input_buffer_frame_start[input_index];

						char label[64] = {0};
						uint32_t label_length = sprintf(label, "%s %s - %u", _get_motion_label(input), _get_action_label(input), input_duration);

						Clay_String label_string = (Clay_String){
							.isStaticallyAllocated = false,
							.length = label_length,
							.chars = ui_string(label, label_length),
						};
						CLAY_TEXT(label_string, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {0, 0, 0, 255} }));
					}
					#endif
				}
			}

		}


	}
}
