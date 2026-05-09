#include "game_mainmenu.h"
#include "ui.h"

void mainmenu_init(struct MainMenu *mainmenu)
{
	printf("[game] MAINMENU: Init\n");
	*mainmenu = (struct MainMenu){0};
}

void mainmenu_term(struct MainMenu *mainmenu)
{
	(void)mainmenu;
	printf("MAINMENU: Term\n");
}

bool mainmenu_update(struct Game *game, struct GameUpdateContext const* ctx)
{
	(void)ctx;
	struct MainMenu *mainmenu = &game->mainmenu;

	if (mainmenu->local_pressed) {
		game->current_state = GAME_STATE_LOCAL_BATTLE;
		local_battle_init(game);
		return true;
	}
	if (mainmenu->network_pressed) {
		game->current_state = GAME_STATE_NETWORK_BATTLE;
		network_battle_init(game);
		return true;
	}
	return false;
}

void mainmenu_render(struct MainMenu *mainmenu)
{
	CLAY({ .id = CLAY_ID("OuterContainer"), .layout = { .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }}) {


		CLAY({
				.id = CLAY_ID("Floating"),
				.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 },
				.floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_CENTER } },
				.backgroundColor = {0, 0, 0, 128}
			}) {

			ui_button("Local battle", &mainmenu->local_pressed);
			if (mainmenu->local_pressed) {
				printf("Local battle!\n");
			}

			ui_button("Network battle", &mainmenu->network_pressed);
			if (mainmenu->network_pressed) {
				printf("Network battle!\n");
			}

			ui_button("Options", NULL);
		}
        }

}
