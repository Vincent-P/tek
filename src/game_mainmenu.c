#include "ui.h"

struct MainMenuState
{
	bool local_pressed;
	bool network_pressed;
};

void mainmenu_init(void **state_data, struct Game const *game)
{
	printf("[game] MAINMENU: Init\n");
	*state_data = calloc(1, sizeof(struct MainMenuState));
}

void mainmenu_term(void **state_data)
{
	printf("MAINMENU: Term\n");
}

struct GameUpdateResult mainmenu_update(void **state_data, struct GameUpdateContext const* ctx)
{
	struct MainMenuState *state = *state_data;
	
	struct GameUpdateResult result = {0};
	if (state->local_pressed) {
		result.action = GAME_UPDATE_ACTION_TRANSITION;
		result.transition_state = GAME_STATE_LOCAL_BATTLE;
	}
	if (state->network_pressed) {
		result.action = GAME_UPDATE_ACTION_TRANSITION;
		result.transition_state = GAME_STATE_NETWORK_BATTLE;
	}
	return result;
}

void mainmenu_render(void **state_data)
{
	struct MainMenuState *state = *state_data;
		
	const Clay_Color COLOR_LIGHT = (Clay_Color) {36, 64, 17, 255};
	const Clay_Color COLOR_RED = (Clay_Color) {168, 66, 28, 255};
	const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};
	uint32_t profilePicture = 0;
	CLAY({ .id = CLAY_ID("OuterContainer"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }, .backgroundColor = {0,0,0,0} }) {
		
		CLAY({
				.id = CLAY_ID("SideBar"),
				.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
				.backgroundColor = COLOR_LIGHT
			}) {

			ui_button("Local battle", &state->local_pressed);
			if (state->local_pressed) {
				printf("Local battle!\n");
			}

			ui_button("Network battle", &state->network_pressed);
			if (state->network_pressed) {
				printf("Network battle!\n");
			}

			ui_button("Options", NULL);
		}
        }

}
