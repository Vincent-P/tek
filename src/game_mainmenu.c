void mainmenu_init(void **state_data, struct Game const *game)
{
	printf("MAINMENU: Init\n");
}

void mainmenu_term(void **state_data)
{
	printf("MAINMENU: Term\n");
}

struct GameUpdateResult mainmenu_update(void **state_data, struct GameUpdateContext const* ctx)
{
	struct GameUpdateResult result = {0};
	if (ImGui_Begin("Main menu", NULL, 0)) {
		if (ImGui_Button("Local battle")) {
			result.action = GAME_UPDATE_ACTION_TRANSITION;
			result.transition_state = GAME_STATE_LOCAL_BATTLE;
		}
		if (ImGui_Button("Network battle")) {
			result.action = GAME_UPDATE_ACTION_TRANSITION;
			result.transition_state = GAME_STATE_NETWORK_BATTLE;
		}
		if (ImGui_Button("Options")) {
		}
	}
	ImGui_End();

	return result;
}

void mainmenu_render(void **state_data)
{
}
