#include "game_battle.h"

#define REPLAY_LENGTH_IN_FRAMES (60*60*60) // 1 hour

enum BattleReplayState
{
	BATTLE_REPLAY_STATE_PLAYING,
	BATTLE_REPLAY_STATE_WATCHING,
	BATTLE_REPLAY_STATE_END,
};

enum BattleReplayPlayingState
{
	BATTLE_REPLAY_PLAYING_STATE_PLAYING,
	BATTLE_REPLAY_PLAYING_STATE_RECORDING,
};

enum BattleReplayWatchingState
{
	BATTLE_REPLAY_WATCHING_STATE_PLAY,
	BATTLE_REPLAY_WATCHING_STATE_PAUSE,
};

struct LocalBattleState
{
	uint64_t accumulator;
	uint64_t t;
	struct BattleContext battle_context;

	// NOTE: The battle context has direct access to assets and renderer.
	// But to support rollback easily, inputs are converted and passed explicitly to the simulation.
	struct Inputs *inputs;
	
	struct Game const *game;

	// replay state
	enum BattleReplayState battle_replay_state;
	struct BattleContext replay_initial_context;
	struct BattleInputs replay_inputs[60*60*60];
	uint32_t replay_current_input;
	uint32_t replay_length;
	// playing state
	enum BattleReplayPlayingState battle_replay_playing_state;
	// watching state
	enum BattleReplayWatchingState battle_replay_watching_state;
	uint32_t battle_replay_watching_frame;
};

void local_battle_new_match(struct LocalBattleState *state)
{
	memset(&state->battle_context, 0, sizeof(state->battle_context));
	
	state->battle_context.assets = state->game->assets;
	state->battle_context.renderer = state->game->renderer;
	state->battle_context.battle_non_state.rounds_first_to = 3;
	battle_state_init(&state->battle_context);
}

void local_battle_init(void **state_data, struct Game const *game)
{
	printf("LOCAL_BATTLE: Init\n");
	*state_data = calloc(1, sizeof(struct LocalBattleState));
	struct LocalBattleState *state = *state_data;
	state->inputs = game->inputs;
	state->game = game;

	local_battle_new_match(state);
}

void local_battle_term(void** state_data)
{
	printf("LOCAL_BATTLE: Term\n");
	struct LocalBattleState *state = *state_data;
	battle_state_term(&state->battle_context);
	
	free(*state_data);
	*state_data = NULL;
}

// -- replay

static void local_battle_start_record_replay(struct LocalBattleState *state)
{
	state->battle_replay_state = BATTLE_REPLAY_STATE_PLAYING;
	state->battle_replay_playing_state = BATTLE_REPLAY_PLAYING_STATE_RECORDING;

	// copy context
	memcpy(&state->replay_initial_context, &state->battle_context, sizeof(struct BattleContext));
	// record inputs
	state->replay_current_input = 0;
}

static void local_battle_stop_record_replay(struct LocalBattleState *state)
{
	state->battle_replay_playing_state = BATTLE_REPLAY_PLAYING_STATE_PLAYING;
	
	state->replay_length = state->replay_current_input;
}

static void local_battle_watch_play_replay(struct LocalBattleState *state);
static void local_battle_watch_set_frame(struct LocalBattleState *state, uint32_t frame);

static void local_battle_watch_replay(struct LocalBattleState *state)
{
	state->battle_replay_state = BATTLE_REPLAY_STATE_WATCHING;

	local_battle_watch_play_replay(state);
	local_battle_watch_set_frame(state, 0);
}

static void local_battle_watch_play_replay(struct LocalBattleState *state)
{
	state->battle_replay_state = BATTLE_REPLAY_STATE_WATCHING;
	
	state->battle_replay_watching_state = BATTLE_REPLAY_WATCHING_STATE_PLAY;
}

static void local_battle_watch_pause_replay(struct LocalBattleState *state)
{
	state->battle_replay_watching_state = BATTLE_REPLAY_WATCHING_STATE_PAUSE;
}

static void local_battle_watch_exit_replay(struct LocalBattleState *state)
{
	state->battle_replay_state = BATTLE_REPLAY_STATE_PLAYING;
}

static void local_battle_watch_set_frame(struct LocalBattleState *state, uint32_t frame)
{
	// clamp frame to last frame
	if (frame >= state->replay_length) {
		frame = state->replay_length - 1; 
	}

	if (frame < state->battle_replay_watching_frame) {
		
		// we are going back in time, resimulate from the initial game state
		memcpy(&state->battle_context, &state->replay_initial_context, sizeof(struct BattleContext));
		for (uint32_t i = 0; i < frame; ++i) {
			TracyCZoneN(f, "Replay Frame", true);
			struct BattleInputs battle_inputs = state->replay_inputs[i];
			enum BattleFrameResult battle_result = battle_simulate_frame(&state->battle_context, battle_inputs);
			TracyCZoneEnd(f);
		}
		
	} else {
		
		// we are going forward in time, we can simulate from here
		for (uint32_t i = state->battle_replay_watching_frame; i < frame; ++i) {
			TracyCZoneN(f, "Replay Frame", true);
			struct BattleInputs battle_inputs = state->replay_inputs[i];
			enum BattleFrameResult battle_result = battle_simulate_frame(&state->battle_context, battle_inputs);
			TracyCZoneEnd(f);
		}

	}
	
	state->battle_replay_watching_frame = frame;
}

// -- 

struct GameUpdateResult local_battle_update(void** state_data, struct GameUpdateContext const *ctx)
{
	struct LocalBattleState *state = *state_data;
	
	// simulate in fixed-step increments from the accumulator
	if (state->battle_replay_state == BATTLE_REPLAY_STATE_END) {
		if (ImGui_Begin("Local Battle", NULL, 0)) {
			if (ImGui_Button("Rematch")) {
				local_battle_new_match(state);
			}
		}
		ImGui_End();		
	} else {
		if (ImGui_Begin("Local Battle", NULL, 0)) {
			ImGui_Text("battle accumulator: %llu", state->accumulator);
			ImGui_Text("battle time: %llu", state->t);
			ImGui_Text("battle frame: %u", state->battle_context.battle_state.frame_number);
			ImGui_Text("replay frame: %u", state->replay_initial_context.battle_state.frame_number + state->battle_replay_watching_frame);
			ImGui_Separator();
			ImGui_Text("Replay");

			if (state->battle_replay_state == BATTLE_REPLAY_STATE_PLAYING) {
				if (state->battle_replay_playing_state == BATTLE_REPLAY_PLAYING_STATE_PLAYING) {
					if (ImGui_Button("Record")) {
						local_battle_start_record_replay(state);
					}
				} else if (state->battle_replay_playing_state == BATTLE_REPLAY_PLAYING_STATE_RECORDING) {
					if (ImGui_Button("Stop")) {
						local_battle_stop_record_replay(state);
					}
				}

				if (state->replay_length > 0) {
					if (ImGui_Button("Watch replay")) {
						local_battle_watch_replay(state);
					}
				}
			} else if (state->battle_replay_state == BATTLE_REPLAY_STATE_WATCHING) {
				ImGui_Text("%u/%u", state->battle_replay_watching_frame, state->replay_length);
				if (ImGui_Button("Exit replay")) {
					local_battle_watch_exit_replay(state);
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
				if (state->battle_replay_watching_state == BATTLE_REPLAY_WATCHING_STATE_PLAY) {
					if (ImGui_Button("pause")) {
						local_battle_watch_pause_replay(state);
					}
				} else if (state->battle_replay_watching_state == BATTLE_REPLAY_WATCHING_STATE_PAUSE) {
					if (ImGui_Button("play")) {
						local_battle_watch_play_replay(state);
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
					uint32_t new_frame = state->battle_replay_watching_frame + advance;
					local_battle_watch_set_frame(state, new_frame);
				}
			}
		}
		ImGui_End();
	}

	switch (state->battle_replay_state) {
	case BATTLE_REPLAY_STATE_PLAYING: {

		state->accumulator += ctx->previous_frame_time;
		const uint64_t dt = 16;
		while (state->accumulator >= dt) {
			TracyCZoneN(f, "Battle Frame", true);
			struct BattleInputs battle_inputs = battle_read_input(state->inputs);
			// ggpo_add_local_input
			// ggpo_synchronize_input
			enum BattleFrameResult battle_result = battle_simulate_frame(&state->battle_context, battle_inputs);
			TracyCZoneEnd(f);

			// save inputs to replay
			if (state->battle_replay_playing_state == BATTLE_REPLAY_PLAYING_STATE_RECORDING) {
				state->replay_inputs[state->replay_current_input] = battle_inputs;
				if (state->replay_current_input + 1 >= ARRAY_LENGTH(state->replay_inputs)) {
					local_battle_stop_record_replay(state);
				} else {
					state->replay_current_input += 1;
				}
			}
			
			state->t += dt;
			state->accumulator -= dt;

			if (battle_result != BATTLE_FRAME_RESULT_CONTINUE) {
				state->battle_replay_state = BATTLE_REPLAY_STATE_END;
				battle_state_term(&state->battle_context);
				break;
			}
		}

		break;
	}
	case BATTLE_REPLAY_STATE_WATCHING: {

		if (state->battle_replay_watching_state == BATTLE_REPLAY_WATCHING_STATE_PLAY) {
			state->accumulator += ctx->previous_frame_time;
			const uint64_t dt = 16;
			while (state->accumulator >= dt) {
				TracyCZoneN(f, "Battle Frame", true);
				struct BattleInputs battle_inputs = state->replay_inputs[state->battle_replay_watching_frame];
				enum BattleFrameResult battle_result = battle_simulate_frame(&state->battle_context, battle_inputs);
				TracyCZoneEnd(f);

				state->battle_replay_watching_frame += 1;
				state->t += dt;
				state->accumulator -= dt;
			}

			if (state->battle_replay_watching_frame >= state->replay_length) {
				local_battle_watch_set_frame(state, 0);
			}
		}
		
		break;
	}
	case BATTLE_REPLAY_STATE_END: {
		break;
	}
	}
	

	struct GameUpdateResult result = {0};
	// TODO: Pause menu
	if (ImGui_Begin("Local Battle", NULL, 0)) {
		if (ImGui_Button("Options")) {
		}
		if (ImGui_Button("Return to main menu")) {
			result.action = GAME_UPDATE_ACTION_TRANSITION;
			result.transition_state = GAME_STATE_MAIN_MENU;
		}
	}
	ImGui_End();

	return result;
}

void local_battle_render(void **state_data)
{
	struct LocalBattleState *state = *state_data;
	if (state->battle_replay_state != BATTLE_REPLAY_STATE_END) {
		battle_render(&state->battle_context);
	}


	float p1_hp_filled = state->battle_context.battle_state.p1_entity.tek.hp * 0.01f;
	float p2_hp_filled = state->battle_context.battle_state.p2_entity.tek.hp * 0.01f;
	
	CLAY({
			.id = CLAY_ID("OuterContainer"),
			.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
			.backgroundColor = {0,0,0,0}
		}) {
		
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

}
