#include "game_battle.h"

struct LocalBattleState
{
	uint64_t accumulator;
	uint64_t t;
	struct BattleContext battle_context;

	// NOTE: The battle context has direct access to assets and renderer.
	// But to support rollback easily, inputs are converted and passed explicitly to the simulation.
	struct Inputs *inputs;
};

void local_battle_init(void **state_data, struct Game const *game)
{
	printf("LOCAL_BATTLE: Init\n");
	*state_data = calloc(1, sizeof(struct LocalBattleState));
	struct LocalBattleState *state = *state_data;
	state->inputs = game->inputs;
		
	// Init battle system
	state->battle_context.assets = game->assets;
	state->battle_context.renderer = game->renderer;
	battle_state_init(&state->battle_context);
}

void local_battle_term(void** state_data)
{
	printf("LOCAL_BATTLE: Term\n");
	struct LocalBattleState *state = *state_data;
	battle_state_term(&state->battle_context);
	
	free(*state_data);
	*state_data = NULL;
}

struct GameUpdateResult local_battle_update(void** state_data, struct GameUpdateContext const *ctx)
{
	struct LocalBattleState *state = *state_data;
	
	// simulate in fixed-step increments from the accumulator
	state->accumulator += ctx->previous_frame_time;
	if (ImGui_Begin("Game", NULL, 0)) {
		ImGui_Text("battle accumulator: %llu", state->accumulator);
		ImGui_Text("battle time: %llu", state->t);
	}
	ImGui_End();

	const uint64_t dt = 16;
	while (state->accumulator >= dt) {
		TracyCZoneN(f, "Battle Frame", true);
		struct BattleInputs battle_inputs = battle_read_input(state->inputs);
		// ggpo_add_local_input
		// ggpo_synchronize_input
		battle_simulate_frame(&state->battle_context, battle_inputs);
		TracyCZoneEnd(f);

		state->t += dt;
		state->accumulator -= dt;
	}

	// TODO: Pause menu
	struct GameUpdateResult result = {0};
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
	battle_render(&state->battle_context);
}
