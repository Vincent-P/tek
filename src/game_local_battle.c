#include "game_battle.h"

#define REPLAY_LENGTH_IN_FRAMES (60*60*60) // 1 hour

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

struct LocalBattleData
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

void local_battle_new_match(struct LocalBattleData *data)
{
	data->state = LOCAL_BATTLE_STATE_PLAYING;
	
	memset(&data->battle_context, 0, sizeof(data->battle_context));

	data->battle_context.assets = data->game->assets;
	data->battle_context.renderer = data->game->renderer;
	data->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&data->battle_context);
}

void local_battle_init(void **state_data, struct Game const *game)
{
	printf("LOCAL_BATTLE: Init\n");
	*state_data = calloc(1, sizeof(struct LocalBattleData));
	struct LocalBattleData *data = *state_data;
	data->inputs = game->inputs;
	data->game = game;

	local_battle_new_match(data);
}

void local_battle_term(void** state_data)
{
	printf("LOCAL_BATTLE: Term\n");
	struct LocalBattleData *data = *state_data;
	battle_state_term(&data->battle_context);

	free(*state_data);
	*state_data = NULL;
}

// -- replay

static void local_battle_watch_set_frame(struct LocalBattleData *data, uint32_t frame)
{
	if (frame >= data->replay_length) {
		if (frame > data->replay_length * 2) {
			frame = data->replay_length - 1;
		} else {
			frame = 0;
		}
	}

	if (frame == 0) {
		memcpy(&data->battle_context, &data->replay_initial_context, sizeof(struct BattleContext));
	}

	if (frame < data->watching_frame) {

		// we are going back in time, resimulate from the initial game data
		memcpy(&data->battle_context, &data->replay_initial_context, sizeof(struct BattleContext));
		for (uint32_t i = 0; i < frame; ++i) {
			TracyCZoneN(f, "Replay Frame", true);
			struct BattleInputs battle_inputs = data->replay_inputs[i];
			enum BattleFrameResult battle_result = battle_simulate_frame(&data->battle_context, battle_inputs);
			TracyCZoneEnd(f);
		}

	} else {

		// we are going forward in time, we can simulate from here
		for (uint32_t i = data->watching_frame; i < frame; ++i) {
			TracyCZoneN(f, "Replay Frame", true);
			struct BattleInputs battle_inputs = data->replay_inputs[i];
			enum BattleFrameResult battle_result = battle_simulate_frame(&data->battle_context, battle_inputs);
			TracyCZoneEnd(f);
		}

	}

	data->watching_frame = frame;
}

// --

struct GameUpdateResult local_battle_update(void** data_data, struct GameUpdateContext const *ctx)
{
	struct LocalBattleData *data = *data_data;

	if (ImGui_Begin("Local Battle", NULL, 0)) {
		ImGui_Text("battle accumulator: %llu", data->accumulator);
		ImGui_Text("battle time: %llu", data->t);
		ImGui_Text("battle frame: %u", data->battle_context.battle_state.frame_number);
		ImGui_Text("replay frame: %u", data->replay_initial_context.battle_state.frame_number + data->watching_frame);
		ImGui_Separator();
		if (ImGui_Button("Pause")) {
			data->pause_previous_state = data->state;
			data->state = LOCAL_BATTLE_STATE_PAUSE;
		}
		ImGui_Separator();
	}
	ImGui_End();

	if (data->state != LOCAL_BATTLE_STATE_PAUSE) {
		if (data->inputs->buttons_is_pressed[InputButtons_Escape] || data->inputs->gamepad_buttons_is_pressed[InputGamepadButtons_START]) {
			data->pause_previous_state = data->state;
			data->state = LOCAL_BATTLE_STATE_PAUSE;
		}
	} else {
		if (data->inputs->buttons_is_pressed[InputButtons_Escape] || data->inputs->gamepad_buttons_is_pressed[InputGamepadButtons_START]) {
			data->pause_resume_pressed = true;
		}
	}

	switch(data->state) {
	case LOCAL_BATTLE_STATE_PLAYING: {
		if (ImGui_Begin("Local Battle", NULL, 0)) {

			ImGui_Text("Replay");

			switch (data->replay_recorder_state) {
			case REPLAY_RECORDER_STATE_INACTIVE: {
				if (ImGui_Button("Record")) {
					data->playing_state = LOCAL_BATTLE_PLAYING_STATE_RECORDING;
					data->replay_recorder_state = REPLAY_RECORDER_STATE_START_RECORD;
				}
				break;
			}
			case REPLAY_RECORDER_STATE_START_RECORD: {
				break;
			}
			case REPLAY_RECORDER_STATE_ACTIVE: {
				if (ImGui_Button("Stop")) {
					data->replay_recorder_state = REPLAY_RECORDER_STATE_STOP_RECORD;
				}
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
			}
		}
		ImGui_End();
		break;
	}

	case LOCAL_BATTLE_STATE_WATCHING: {
		if (ImGui_Begin("Local Battle", NULL, 0)) {
			ImGui_Text("Replay");
			ImGui_Text("%u/%u", data->watching_frame, data->replay_length);
			if (ImGui_Button("Exit replay")) {
				data->replay_watcher_state = REPLAY_WATCHER_STATE_EXIT;
			}

			int advance = 0;
			if (ImGui_Button("<<<")) {
				advance = -4;
			}
			ImGui_SameLine();
			if (ImGui_Button("<<")) {
				advance = -2;
			}
			ImGui_SameLine();
			if (ImGui_Button("<")) {
				advance = -1;
			}
			ImGui_SameLine();
			if (data->replay_watcher_state == REPLAY_WATCHER_STATE_PLAYING) {
				if (ImGui_Button("pause")) {
					data->replay_watcher_state = REPLAY_WATCHER_STATE_PAUSED;
				}
			} else if (data->replay_watcher_state == REPLAY_WATCHER_STATE_PAUSED) {
				if (ImGui_Button("play")) {
					data->replay_watcher_state = REPLAY_WATCHER_STATE_START_PLAY;
				}
			}
			ImGui_SameLine();
			if (ImGui_Button(">")) {
				advance = 1;
			}
			ImGui_SameLine();
			if (ImGui_Button(">>")) {
				advance = 2;
			}
			ImGui_SameLine();
			if (ImGui_Button(">>>")) {
				advance = 4;
			}

			if (advance != 0) {
				uint32_t new_frame = data->watching_frame + advance;
				local_battle_watch_set_frame(data, new_frame);
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
				local_battle_new_match(data);
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
				memcpy(&data->replay_initial_context, &data->battle_context, sizeof(struct BattleContext));
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
		data->accumulator += ctx->previous_frame_time;
		const uint64_t dt = 16;
		while (data->accumulator >= dt) {
			TracyCZoneN(f, "Battle Frame", true);
			struct BattleInputs battle_inputs = battle_read_input(data->inputs);
			// ggpo_add_local_input
			// ggpo_synchronize_input
			enum BattleFrameResult battle_result = battle_simulate_frame(&data->battle_context, battle_inputs);
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

			data->t += dt;
			data->accumulator -= dt;

			if (battle_result != BATTLE_FRAME_RESULT_CONTINUE) {
				data->state = LOCAL_BATTLE_STATE_END;
				battle_state_term(&data->battle_context);
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
			local_battle_watch_set_frame(data, 0);
			break;
		}

		case REPLAY_WATCHER_STATE_PLAYING: {
			// Watching replay. Read inputs from buffer and simulate battle.
			data->accumulator += ctx->previous_frame_time;
			const uint64_t dt = 16;
			while (data->accumulator >= dt) {
				TracyCZoneN(f, "Battle Frame", true);
				struct BattleInputs battle_inputs = data->replay_inputs[data->watching_frame];
				enum BattleFrameResult battle_result = battle_simulate_frame(&data->battle_context, battle_inputs);
				TracyCZoneEnd(f);

				data->watching_frame += 1;
				data->t += dt;
				data->accumulator -= dt;
			}

			if (data->watching_frame >= data->replay_length) {
				local_battle_watch_set_frame(data, 0);
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


	struct GameUpdateResult result = {0};
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
			result.action = GAME_UPDATE_ACTION_TRANSITION;
			result.transition_state = GAME_STATE_MAIN_MENU;
		}

		data->pause_resume_pressed = false;
		data->pause_options_pressed = false;
		data->pause_mainmenu_pressed = false;
		break;
	}

	case LOCAL_BATTLE_STATE_END: {
		if (data->end_rematch_pressed) {
			local_battle_new_match(data);
		} else if (data->end_mainmenu_pressed) {
			result.action = GAME_UPDATE_ACTION_TRANSITION;
			result.transition_state = GAME_STATE_MAIN_MENU;
		}

		data->end_rematch_pressed = false;
		data->end_mainmenu_pressed = false;
		break;
	}
	}

	return result;
}

void local_battle_render(void **data_data)
{
	struct LocalBattleData *data = *data_data;
	if (data->state != LOCAL_BATTLE_STATE_END) {
		battle_render(&data->battle_context);
	}


	float p1_hp_filled = data->battle_context.battle_state.p1_entity.tek.hp * 0.01f;
	float p2_hp_filled = data->battle_context.battle_state.p2_entity.tek.hp * 0.01f;

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

					CLAY_TEXT(CLAY_STRING("SuperBob"), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {255, 255, 255, 255}, .textAlignment = CLAY_TEXT_ALIGN_LEFT }));
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

					CLAY_TEXT(CLAY_STRING("Seb"), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {255, 255, 255, 255}, .textAlignment = CLAY_TEXT_ALIGN_RIGHT }));

				}


			}
		}

		// pause menu
		if (data->state == LOCAL_BATTLE_STATE_PAUSE) {
			CLAY({.id = CLAY_ID("PAUSE_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
				CLAY({
						.id = CLAY_ID("SideBar"),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.backgroundColor = {128, 128, 128, 255},
					}) {

					ui_button("Resume", &data->pause_resume_pressed);
					ui_button("Options", &data->pause_options_pressed);
					ui_button("Return to main menu", &data->pause_mainmenu_pressed);
				}
			}
		}

		if (data->state == LOCAL_BATTLE_STATE_END) {
			CLAY({.id = CLAY_ID("END_FRAME"), .floating = { .attachTo = CLAY_ATTACH_TO_PARENT }, .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {0, 0, 0, 128}} ) {
				CLAY({
						.id = CLAY_ID("SideBar"),
						.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
						.backgroundColor = {128, 128, 128, 255},
					}) {

					ui_button("Rematch", &data->end_rematch_pressed);
					ui_button("Return to main menu", &data->end_mainmenu_pressed);
				}
			}
		}
	}

}
