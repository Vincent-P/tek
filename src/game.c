#include "game.h"

void game_init(struct Game *game)
{
	STATE_INIT_FUNCTIONS[game->current_state](&game->state_data, game);
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
