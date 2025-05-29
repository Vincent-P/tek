void network_battle_init(void **state_data, struct Game const *game)
{
	printf("NETWORK_BATTLE: Init\n");
}

void network_battle_term(void **state_data)
{
	printf("NETWORK_BATTLE: Term\n");
}

struct GameUpdateResult network_battle_update(void **state_data, struct GameUpdateContext const *ctx)
{
	struct GameUpdateResult result = {0};
	result.action = GAME_UPDATE_ACTION_TRANSITION;
	result.transition_state = GAME_STATE_MAIN_MENU;
	return result;
}

void network_battle_render(void **state_data)
{
}
