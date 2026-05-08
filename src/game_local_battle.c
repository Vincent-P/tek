#include "game_local_battle.h"
#include "game_battle.h"


/**
States:
(- Side select)
(- Map select)
(- Character select)
- Round
  - Playing
    - Playing
    - Recording replay
  - Watching replay
    - Playback
    - Paused
  - Pause menu
- Match end
 **/

void local_battle_new_match(struct Game *game)
{
	struct LocalBattle *data = &game->local_battle;
	struct Simulation *simulation = &game->simulation;
	data->state = LOCAL_BATTLE_STATE_PLAYING;

	memset(&simulation->battle_context, 0, sizeof(simulation->battle_context));
	simulation->battle_context.assets = game->assets;
	simulation->battle_context.renderer = game->renderer;
	simulation->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&simulation->battle_context);
}

void local_battle_init(struct Game *game)
{
	printf("LOCAL_BATTLE: Init\n");
	memset(&game->local_battle, 0, sizeof(struct LocalBattle));
	local_battle_new_match(game);
}

static void _exit_local_battle(struct Game *game)
{
	printf("LOCAL_BATTLE: Exit to mainmenu\n");

	battle_state_term(&game->simulation.battle_context);

	game->current_state = GAME_STATE_MAIN_MENU;
	memset(&game->mainmenu, 0, sizeof(struct MainMenu));
}

// -- replay

static void local_battle_watch_set_frame(struct Game *game, uint32_t frame)
{
	struct LocalBattle *data = &game->local_battle;
	struct Simulation *simulation = &game->simulation;

	if (frame >= data->replay_length) {
		if (frame > data->replay_length * 2) {
			frame = data->replay_length - 1;
		} else {
			frame = 0;
		}
	}

	if (frame == 0) {
		memcpy(&simulation->battle_context, &data->replay_initial_context, sizeof(struct BattleContext));
	}

	if (frame < data->watching_frame) {

		// we are going back in time, resimulate from the initial game data
		memcpy(&simulation->battle_context, &data->replay_initial_context, sizeof(struct BattleContext));
		for (uint32_t i = 0; i < frame; ++i) {
			TracyCZoneN(f, "Replay Frame", true);
			struct BattleInputs battle_inputs = data->replay_inputs[i];
			enum BattleFrameResult battle_result = battle_simulate_frame(&simulation->battle_context, battle_inputs);
			TracyCZoneEnd(f);
		}

	} else {

		// we are going forward in time, we can simulate from here
		for (uint32_t i = data->watching_frame; i < frame; ++i) {
			TracyCZoneN(f, "Replay Frame", true);
			struct BattleInputs battle_inputs = data->replay_inputs[i];
			enum BattleFrameResult battle_result = battle_simulate_frame(&simulation->battle_context, battle_inputs);
			TracyCZoneEnd(f);
		}

	}

	data->watching_frame = frame;
}

// --

bool local_battle_update(struct Game *game, struct GameUpdateContext const *ctx)
{
	struct Inputs const* inputs = game->inputs;
	struct LocalBattle *data = &game->local_battle;
	struct Simulation *simulation = &game->simulation;

	if (ImGui_Begin("Local Battle", NULL, 0)) {
		if (ImGui_Button("Pause")) {
			data->pause_previous_state = data->state;
			data->state = LOCAL_BATTLE_STATE_PAUSE;
		}
		ImGui_SameLine();
		ImGui_TextUnformatted("|");
		ImGui_SameLine();
	}
	ImGui_End();

	if (data->state != LOCAL_BATTLE_STATE_PAUSE) {
		if (inputs->buttons_was_pressed[InputButtons_Escape]
		    || inputs->gamepad_buttons_is_down[InputGamepadButtons_START]) {
			data->pause_previous_state = data->state;
			data->state = LOCAL_BATTLE_STATE_PAUSE;
		}
	} else {
		if (inputs->buttons_was_pressed[InputButtons_Escape]
		    || inputs->gamepad_buttons_is_down[InputGamepadButtons_START]) {
			data->pause_resume_pressed = true;
		}
	}

	switch(data->state) {
	case LOCAL_BATTLE_STATE_PLAYING: {
		if (ImGui_Begin("Local Battle", NULL, 0)) {

			ImGui_Text("Replay:");
			ImGui_SameLine();

			switch (data->replay_recorder_state) {
			case REPLAY_RECORDER_STATE_INACTIVE: {
				if (ImGui_Button("Record")) {
					data->playing_state = LOCAL_BATTLE_PLAYING_STATE_RECORDING;
					data->replay_recorder_state = REPLAY_RECORDER_STATE_START_RECORD;
				}
				ImGui_SameLine();
				break;
			}
			case REPLAY_RECORDER_STATE_START_RECORD: {
				break;
			}
			case REPLAY_RECORDER_STATE_ACTIVE: {
				if (ImGui_Button("Stop")) {
					data->replay_recorder_state = REPLAY_RECORDER_STATE_STOP_RECORD;
				}
				ImGui_SameLine();
				break;
			}
			case REPLAY_RECORDER_STATE_STOP_RECORD: {
				break;
			}
			}

			if (data->replay_length > 0) {
				if (ImGui_Button("Watch replay")) {
					data->state = LOCAL_BATTLE_STATE_WATCHING;
					data->replay_watcher_state = REPLAY_WATCHER_STATE_START_PLAY;
				}
				ImGui_SameLine();
			}
		}
		ImGui_End();
		break;
	}

	case LOCAL_BATTLE_STATE_WATCHING: {
		if (ImGui_Begin("Local Battle", NULL, 0)) {
			ImGui_SameLine();
			ImGui_Text("Replay:");
			ImGui_SameLine();
			ImGui_Text("%u/%u", data->watching_frame, data->replay_length);
			ImGui_SameLine();
			if (ImGui_Button("Exit replay")) {
				data->replay_watcher_state = REPLAY_WATCHER_STATE_EXIT;
			}
			ImGui_SameLine();

			const float button_width = 100.0f;
			int advance = 0;
			if (ImGui_ButtonEx("<<<", (ImVec2){button_width, 0})) {
				advance = -4;
			}
			ImGui_SameLine();
			if (ImGui_ButtonEx("<<", (ImVec2){button_width, 0})) {
				advance = -2;
			}
			ImGui_SameLine();
			if (ImGui_ButtonEx("<", (ImVec2){button_width, 0})) {
				advance = -1;
			}
			ImGui_SameLine();
			if (data->replay_watcher_state == REPLAY_WATCHER_STATE_PLAYING) {
				if (ImGui_ButtonEx("pause", (ImVec2){button_width, 0})) {
					data->replay_watcher_state = REPLAY_WATCHER_STATE_PAUSED;
				}
			} else if (data->replay_watcher_state == REPLAY_WATCHER_STATE_PAUSED) {
				if (ImGui_ButtonEx("play", (ImVec2){button_width, 0})) {
					data->replay_watcher_state = REPLAY_WATCHER_STATE_START_PLAY;
				}
			}
			ImGui_SameLine();
			if (ImGui_ButtonEx(">", (ImVec2){button_width, 0})) {
				advance = 1;
			}
			ImGui_SameLine();
			if (ImGui_ButtonEx(">>", (ImVec2){button_width, 0})) {
				advance = 2;
			}
			ImGui_SameLine();
			if (ImGui_ButtonEx(">>>", (ImVec2){button_width, 0})) {
				advance = 4;
			}

			if (advance != 0) {
				uint32_t new_frame = data->watching_frame + advance;
				local_battle_watch_set_frame(game, new_frame);
			}
		}
		ImGui_End();
		break;
	}

	case LOCAL_BATTLE_STATE_PAUSE: {
		break;
	}

	case LOCAL_BATTLE_STATE_END: {
		if (ImGui_Begin("Local Battle", NULL, 0)) {
			if (ImGui_Button("Rematch")) {
				local_battle_new_match(game);
			}
		}
		ImGui_End();
		break;
	}
	}

	switch (data->state) {
	case LOCAL_BATTLE_STATE_PLAYING: {

		if (data->playing_state == LOCAL_BATTLE_PLAYING_STATE_RECORDING) {
			switch (data->replay_recorder_state) {
			case REPLAY_RECORDER_STATE_INACTIVE: {
				break;
			}
			case REPLAY_RECORDER_STATE_START_RECORD: {
				data->replay_recorder_state = REPLAY_RECORDER_STATE_ACTIVE;
				// copy context
				memcpy(&data->replay_initial_context, &simulation->battle_context, sizeof(struct BattleContext));
				// record inputs
				data->replay_current_input = 0;
				break;

			}
			case REPLAY_RECORDER_STATE_ACTIVE: {
				break;
			}
			case REPLAY_RECORDER_STATE_STOP_RECORD: {
				data->playing_state = LOCAL_BATTLE_PLAYING_STATE_PLAYING;
				data->replay_recorder_state = REPLAY_RECORDER_STATE_INACTIVE;

				data->replay_length = data->replay_current_input;
				break;
			}
			}
		}


		// Playing. Read inputs and simulate battle.
		simulation->accumulator += ctx->previous_frame_time;
		const uint64_t dt = 16;
		while (simulation->accumulator >= dt) {
			TracyCZoneN(f, "Battle Frame", true);
			struct BattleInputs battle_inputs = battle_read_input(inputs);
			// ggpo_add_local_input
			// ggpo_synchronize_input
			enum BattleFrameResult battle_result = battle_simulate_frame(&simulation->battle_context, battle_inputs);
			TracyCZoneEnd(f);

			// save inputs to replay
			bool is_recording = data->playing_state == LOCAL_BATTLE_PLAYING_STATE_RECORDING && data->replay_recorder_state == REPLAY_RECORDER_STATE_ACTIVE;
			if (is_recording) {
				data->replay_inputs[data->replay_current_input] = battle_inputs;
				if (data->replay_current_input + 1 >= ARRAY_LENGTH(data->replay_inputs)) {
					data->replay_recorder_state = REPLAY_RECORDER_STATE_STOP_RECORD;
				} else {
					data->replay_current_input += 1;
				}
			}

			simulation->t += dt;
			simulation->accumulator -= dt;

			if (battle_result != BATTLE_FRAME_RESULT_CONTINUE) {
				data->state = LOCAL_BATTLE_STATE_END;
				battle_state_term(&simulation->battle_context);
				break;
			}
		}

		break;
	}

	case LOCAL_BATTLE_STATE_WATCHING: {
		switch (data->replay_watcher_state) {
		case REPLAY_WATCHER_STATE_START_PLAY: {
			// Start playing
			data->replay_watcher_state = REPLAY_WATCHER_STATE_PLAYING;
			local_battle_watch_set_frame(game, 0);
			break;
		}

		case REPLAY_WATCHER_STATE_PLAYING: {
			// Watching replay. Read inputs from buffer and simulate battle.
			simulation->accumulator += ctx->previous_frame_time;
			const uint64_t dt = 16;
			while (simulation->accumulator >= dt) {
				TracyCZoneN(f, "Battle Frame", true);
				struct BattleInputs battle_inputs = data->replay_inputs[data->watching_frame];
				enum BattleFrameResult battle_result = battle_simulate_frame(&simulation->battle_context, battle_inputs);
				TracyCZoneEnd(f);

				data->watching_frame += 1;
				simulation->t += dt;
				simulation->accumulator -= dt;
			}

			if (data->watching_frame >= data->replay_length) {
				local_battle_watch_set_frame(game, 0);
			}
			break;
		}

		case REPLAY_WATCHER_STATE_PAUSED: {
			// Paused.
			break;
		}

		case REPLAY_WATCHER_STATE_EXIT: {
			// Go back to playing state
			data->replay_watcher_state = REPLAY_WATCHER_STATE_PAUSED;
			data->state = LOCAL_BATTLE_STATE_PLAYING;
			break;
		}
		}

		break;
	}

	case LOCAL_BATTLE_STATE_PAUSE: {
		// game is paused.
		break;
	}

	case LOCAL_BATTLE_STATE_END: {
		break;
	}
	}


	bool result = false;
	switch (data->state) {
	case LOCAL_BATTLE_STATE_PLAYING: {
		break;
	}

	case LOCAL_BATTLE_STATE_WATCHING: {
		break;
	}

	case LOCAL_BATTLE_STATE_PAUSE: {
		if (data->pause_resume_pressed) {
			data->state = data->pause_previous_state;
			assert(data->state != LOCAL_BATTLE_STATE_PAUSE);
		} else if (data->pause_options_pressed) {
		} else if (data->pause_mainmenu_pressed) {
			_exit_local_battle(game);
			result = true;
		}

		data->pause_resume_pressed = false;
		data->pause_options_pressed = false;
		data->pause_mainmenu_pressed = false;
		break;
	}

	case LOCAL_BATTLE_STATE_END: {
		if (data->end_rematch_pressed) {
			local_battle_new_match(game);
		} else if (data->end_mainmenu_pressed) {
			_exit_local_battle(game);
			result = true;
		}

		data->end_rematch_pressed = false;
		data->end_mainmenu_pressed = false;
		break;
	}
	}

	return result;
}

void local_battle_render(struct Game *game)
{
	struct LocalBattle *data = &game->local_battle;
	struct Simulation *simulation = &game->simulation;
	if (data->state != LOCAL_BATTLE_STATE_END) {
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

		if (data->state == LOCAL_BATTLE_STATE_PLAYING || data->state == LOCAL_BATTLE_STATE_WATCHING) {
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

					uint32_t count = INPUT_BUFFER_SIZE;
					if (p1->input_buffer_head < count) {
						count = p1->input_buffer_head;
					}

					uint32_t it = p1->input_buffer_head-1;
					uint32_t input_index = it % INPUT_BUFFER_SIZE;
					struct BattleInput input = p1->input_buffer[input_index];
					uint32_t frame = battle_state->frame_number;
					for (uint32_t i = 0; i < count; i++, it--) {
						uint32_t input_index = it % INPUT_BUFFER_SIZE;
						if (p1->input_buffer[input_index].motion == input.motion
						    && p1->input_buffer[input_index].actions == input.actions) {
							// Input is the same from the previous one
						} else {
							// Input is different, display
							struct BattleInput next_input = p1->input_buffer[input_index];
							uint32_t next_frame = battle_state->frame_number - i;

							uint32_t duration = frame - next_frame;
							char label[64] = {0};
							uint32_t label_length = sprintf(label, "%s %s - %u", _get_motion_label(input), _get_action_label(input), duration);

							Clay_String label_string = (Clay_String){
								.isStaticallyAllocated = false,
								.length = label_length,
								.chars = ui_string(label, label_length),
							};
							CLAY_TEXT(label_string, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {0, 0, 0, 255}}));

							input = next_input;
							frame = next_frame;
						}
						if (it == 0) {
							break;
						}
					}
					// Last input
					uint32_t duration = frame - (battle_state->frame_number - count);
					char label[64] = {0};
					uint32_t label_length = sprintf(label, "%s %s - %u", _get_motion_label(input), _get_action_label(input), duration);
					Clay_String label_string = (Clay_String){
						.isStaticallyAllocated = false,
						.length = label_length,
						.chars = ui_string(label, label_length),
					};
					CLAY_TEXT(label_string, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {0, 0, 0, 255}}));
				}

				CLAY({.id = CLAY_ID("Empty"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {}

				CLAY({
						.id = CLAY_IDI("PlayerInput", 2),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16, .childAlignment = {.x = CLAY_ALIGN_X_RIGHT} },
					}) {
					struct BattleState* battle_state = &simulation->battle_context.battle_state;
					struct TekPlayerComponent const *p2 = &battle_state->p2_entity.tek;
					uint32_t count = INPUT_BUFFER_SIZE;
					if (p2->input_buffer_head < count) {
						count = p2->input_buffer_head;
					}

					uint32_t it = p2->input_buffer_head-1;
					uint32_t input_index = it % INPUT_BUFFER_SIZE;
					struct BattleInput input = p2->input_buffer[input_index];
					uint32_t frame = battle_state->frame_number;
					for (uint32_t i = 0; i < count; i++, it--) {
						uint32_t input_index = it % INPUT_BUFFER_SIZE;
						if (p2->input_buffer[input_index].motion == input.motion
						    && p2->input_buffer[input_index].actions == input.actions) {
							// Input is the same from the previous one
						} else {
							// Input is different, display
							struct BattleInput next_input = p2->input_buffer[input_index];
							uint32_t next_frame = battle_state->frame_number - i;

							uint32_t duration = frame - next_frame;
							char label[64] = {0};
							uint32_t label_length = sprintf(label, "%s %s - %u", _get_motion_label(input), _get_action_label(input), duration);

							Clay_String label_string = (Clay_String){
								.isStaticallyAllocated = false,
								.length = label_length,
								.chars = ui_string(label, label_length),
							};
							CLAY_TEXT(label_string, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {0, 0, 0, 255}}));

							input = next_input;
							frame = next_frame;
						}
						if (it == 0) {
							break;
						}
					}
					// Last input
					uint32_t duration = frame - (battle_state->frame_number - count);
					char label[64] = {0};
					uint32_t label_length = sprintf(label, "%s %s - %u", _get_motion_label(input), _get_action_label(input), duration);
					Clay_String label_string = (Clay_String){
						.isStaticallyAllocated = false,
						.length = label_length,
						.chars = ui_string(label, label_length),
					};
					CLAY_TEXT(label_string, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = {0, 0, 0, 255}}));
				}
			}

		}

		// pause menu
		if (data->state == LOCAL_BATTLE_STATE_PAUSE) {
			CLAY({
					.id = CLAY_ID("Floating"),
					.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIT(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
					.floating = { .offset = {0, 16}, .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_CENTER } },
					.backgroundColor = {0, 0, 0, 128}
				}) {

				ui_button("Resume", &data->pause_resume_pressed);
				ui_button("Options", &data->pause_options_pressed);
				ui_button("Return to main menu", &data->pause_mainmenu_pressed);
			}
		}

		if (data->state == LOCAL_BATTLE_STATE_END) {
			CLAY({.id = CLAY_ID("END_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
				CLAY({
						.id = CLAY_ID("Floating"),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIT(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.floating = { .offset = {0, 16}, .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_CENTER } },
						.backgroundColor = {0, 0, 0, 128}
					}) {
					ui_button("Rematch", &data->end_rematch_pressed);
					ui_button("Return to main menu", &data->end_mainmenu_pressed);
				}
			}
		}
	}

}
