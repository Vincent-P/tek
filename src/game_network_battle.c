#include <ggponet.h>

#define TEK_NETWORK_HOST_PORT 35584
#define TEK_NETWORK_PEER_PORT 35585

enum NetworkBattleState
{
	NETWORK_BATTLE_STATE_PRELOBBY,
	NETWORK_BATTLE_STATE_HOSTING,
	NETWORK_BATTLE_STATE_JOINING,
};

struct NetworkBattleData
{
	// External data
	// NOTE: The battle context has direct access to assets and renderer.
	// But to support rollback easily, inputs are converted and passed explicitly to the simulation.
	struct Inputs *inputs;
	struct Game const *game;

	// Battle data
	GGPOSession *ggpo_session;
	GGPOPlayer ggpo_players[2];
	GGPOPlayerHandle ggpo_player_handles[2];
	uint64_t accumulator;
	uint64_t t;
	struct BattleContext battle_context;

	// State data
	enum NetworkBattleState state;

	// UI data
	bool host_pressed;
	bool join_pressed;
};

// -- ggpo callbacks
struct NetworkBattleData *ggpo_tek_global_state = NULL;


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
	memcpy(gs_copy, &ggpo_tek_global_state->battle_context.battle_state, length);

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
		memcpy(&ggpo_tek_global_state->battle_context.battle_state, buffer, length);
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
		ggpo_log(ggpo_tek_global_state->ggpo_session, "tek frame: %u", gs->frame_number);
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
	GGPOSession* session = ggpo_tek_global_state->ggpo_session;
	struct BattleContext *battle_ctx = &ggpo_tek_global_state->battle_context;

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



// --

void network_battle_state_host(struct NetworkBattleData *data)
{
	data->state = NETWORK_BATTLE_STATE_HOSTING;

	ggpo_tek_global_state = data;
	data->ggpo_players[0].size = sizeof(GGPOPlayer);
	data->ggpo_players[0].type = GGPO_PLAYERTYPE_LOCAL;
	data->ggpo_players[0].player_num = 1;
	data->ggpo_players[0].u.local.padding = 0;
	data->ggpo_players[1].size = sizeof(GGPOPlayer);
	data->ggpo_players[1].type = GGPO_PLAYERTYPE_REMOTE;
	data->ggpo_players[1].player_num = 2;
	memcpy(data->ggpo_players[1].u.remote.ip_address, "127.0.0.1", 9);
	data->ggpo_players[1].u.remote.port = TEK_NETWORK_PEER_PORT;
	GGPOSessionCallbacks session_callbacks = {0};
	session_callbacks.begin_game = tek_begin_game;
	session_callbacks.save_game_state = tek_save_game_state;
	session_callbacks.load_game_state = tek_load_game_state;
	session_callbacks.log_game_state = tek_log_game_state;
	session_callbacks.free_buffer = tek_free_buffer;
	session_callbacks.advance_frame = tek_advance_frame;
	session_callbacks.on_event = tek_on_event;
	GGPOErrorCode err = ggpo_start_session(&data->ggpo_session, &session_callbacks, "tek", 2, sizeof(BattleInput), TEK_NETWORK_HOST_PORT);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[0], &data->ggpo_player_handles[0]);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[1], &data->ggpo_player_handles[1]);
	tek_check_error(err);

	// Init battle
	memset(&data->battle_context, 0, sizeof(data->battle_context));
	data->battle_context.assets = data->game->assets;
	data->battle_context.renderer = data->game->renderer;
	data->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&data->battle_context);
}

void network_battle_state_host_term(struct NetworkBattleData *data)
{
	battle_state_term(&data->battle_context);

	GGPOErrorCode err = ggpo_close_session(data->ggpo_session);
	tek_check_error(err);
	data->ggpo_session = NULL;
	ggpo_tek_global_state = NULL;
}

void network_battle_state_join(struct NetworkBattleData *data)
{
	data->state = NETWORK_BATTLE_STATE_JOINING;

	ggpo_tek_global_state = data;
	data->ggpo_players[0].size = sizeof(GGPOPlayer);
	data->ggpo_players[0].type = GGPO_PLAYERTYPE_REMOTE;
	data->ggpo_players[0].player_num = 1;
	memcpy(data->ggpo_players[0].u.remote.ip_address, "127.0.0.1", 9);
	data->ggpo_players[0].u.remote.port = TEK_NETWORK_HOST_PORT;
	data->ggpo_players[1].size = sizeof(GGPOPlayer);
	data->ggpo_players[1].type = GGPO_PLAYERTYPE_LOCAL;
	data->ggpo_players[1].player_num = 2;
	data->ggpo_players[1].u.local.padding = 0;
	GGPOSessionCallbacks session_callbacks = {0};
	session_callbacks.begin_game = tek_begin_game;
	session_callbacks.save_game_state = tek_save_game_state;
	session_callbacks.load_game_state = tek_load_game_state;
	session_callbacks.log_game_state = tek_log_game_state;
	session_callbacks.free_buffer = tek_free_buffer;
	session_callbacks.advance_frame = tek_advance_frame;
	session_callbacks.on_event = tek_on_event;
	GGPOErrorCode err = ggpo_start_session(&data->ggpo_session, &session_callbacks, "tek", 2, sizeof(BattleInput), TEK_NETWORK_PEER_PORT);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[0], &data->ggpo_player_handles[0]);
	tek_check_error(err);
	err = ggpo_add_player(data->ggpo_session, &data->ggpo_players[1], &data->ggpo_player_handles[1]);
	tek_check_error(err);

	// Init battle
	memset(&data->battle_context, 0, sizeof(data->battle_context));
	data->battle_context.assets = data->game->assets;
	data->battle_context.renderer = data->game->renderer;
	data->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&data->battle_context);
}

void network_battle_state_join_term(struct NetworkBattleData *data)
{
	battle_state_term(&data->battle_context);

	GGPOErrorCode err = ggpo_close_session(data->ggpo_session);
	tek_check_error(err);
	data->ggpo_session = NULL;
	ggpo_tek_global_state = NULL;
}

// --

void network_battle_init(void **state_data, struct Game const *game)
{
	printf("NETWORK_BATTLE: Init\n");

	printf("NETWORK_BATTLE: Init\n");
	*state_data = calloc(1, sizeof(struct NetworkBattleData));
	struct NetworkBattleData *data = *state_data;
	data->inputs = game->inputs;
	data->game = game;

	ggpo_log(NULL, "test");
}

void network_battle_term(void **state_data)
{
	printf("NETWORK_BATTLE: Term\n");
	struct NetworkBattleData *data = *state_data;

	switch (data->state) {
	case NETWORK_BATTLE_STATE_PRELOBBY: {
		break;
	}

	case NETWORK_BATTLE_STATE_HOSTING: {
		network_battle_state_host_term(data);
		break;
	}
	case NETWORK_BATTLE_STATE_JOINING: {
		network_battle_state_join_term(data);
		break;
	}
	}

	free(*state_data);
	*state_data = NULL;
}

struct GameUpdateResult network_battle_update(void **state_data, struct GameUpdateContext const *ctx)
{
	struct NetworkBattleData *data = *state_data;
	struct GameUpdateResult result = {0};
	switch (data->state) {
	case NETWORK_BATTLE_STATE_PRELOBBY: {

		if (data->host_pressed) {
			network_battle_state_host(data);
			data->host_pressed = false;
		}
		if (data->join_pressed) {
			network_battle_state_join(data);
			data->join_pressed = false;
		}
		break;
	}

	case NETWORK_BATTLE_STATE_HOSTING:
	case NETWORK_BATTLE_STATE_JOINING: {


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
		data->accumulator += ctx->previous_frame_time;
		const uint64_t dt = 16;
		while (data->accumulator >= dt) {
			TracyCZoneN(f, "Battle Frame", true);
			struct BattleInputs battle_inputs = battle_read_input(data->inputs);
			if (data->state == NETWORK_BATTLE_STATE_HOSTING) {
				// Send player1 inputs to network
				err = ggpo_add_local_input(data->ggpo_session, data->ggpo_player_handles[0], &battle_inputs.player1, sizeof(BattleInput));
			} else {
				// Send player2 inputs to network
				err = ggpo_add_local_input(data->ggpo_session, data->ggpo_player_handles[1], &battle_inputs.player1, sizeof(BattleInput));
			}
			if (GGPO_SUCCEEDED(err)) {
				tek_check_error(err);
				// Receive both inputs from network
				struct BattleInputs network_inputs = {0};
				int disconnect_flags = 0;
				err = ggpo_synchronize_input(data->ggpo_session, &network_inputs, sizeof(network_inputs), &disconnect_flags);
				if (GGPO_SUCCEEDED(err)) {
					tek_check_error(err);
					enum BattleFrameResult battle_result = battle_simulate_frame(&data->battle_context, network_inputs);
					TracyCZoneEnd(f);

					err = ggpo_advance_frame(data->ggpo_session);
					tek_check_error(err);

					if (battle_result != BATTLE_FRAME_RESULT_CONTINUE || disconnect_flags != 0) {

						if (data->state == NETWORK_BATTLE_STATE_HOSTING) {
							network_battle_state_host_term(data);
						} else if (data->state == NETWORK_BATTLE_STATE_JOINING) {
							network_battle_state_join_term(data);
						}
						data->state = NETWORK_BATTLE_STATE_PRELOBBY;
						break;
					}
				}
			}

			data->t += dt;
			data->accumulator -= dt;
		}
		break;
	}
	}

	return result;
}

void network_battle_render(void **state_data)
{
	struct NetworkBattleData *data = *state_data;

	if (data->state == NETWORK_BATTLE_STATE_HOSTING || data->state == NETWORK_BATTLE_STATE_JOINING) {
		battle_render(&data->battle_context);
	}

	float p1_hp_filled = data->battle_context.battle_state.p1_entity.tek.hp * 0.01f;
	float p2_hp_filled = data->battle_context.battle_state.p2_entity.tek.hp * 0.01f;
	int rounds_first_to = data->battle_context.battle_non_state.rounds_first_to;
	int rounds_p1_won = data->battle_context.battle_non_state.rounds_p1_won;
	int rounds_p2_won = data->battle_context.battle_non_state.rounds_p2_won;

	CLAY({
			.id = CLAY_ID("OuterContainer"),
			.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
			.backgroundColor = {0,0,0,0}
		}) {

		if (data->state == NETWORK_BATTLE_STATE_HOSTING || data->state == NETWORK_BATTLE_STATE_JOINING) {
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
					struct BattleState* battle_state = &data->battle_context.battle_state;
					struct TekPlayerComponent const *p1 = &battle_state->p1_entity.tek;

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
				}

				CLAY({.id = CLAY_ID("Empty"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {}

				CLAY({
						.id = CLAY_IDI("PlayerInput", 2),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = {.x = CLAY_ALIGN_X_RIGHT} },
					}) {
					struct BattleState* battle_state = &data->battle_context.battle_state;
					struct TekPlayerComponent const *p2 = &battle_state->p2_entity.tek;

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
				}
			}

		}


		if (data->state == NETWORK_BATTLE_STATE_PRELOBBY) {
			CLAY({.id = CLAY_ID("PRELOBBY_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
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
		}


	}
}
