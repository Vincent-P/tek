#include "game_mainmenu.h"
#include "ui_helpers.h"


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

static bool _mainmenu_render_button(UiHierarchy *h, const char *label)
{
	float FONT_SIZE = ADJUST_FLOAT(48.0f);
	int COLOR = ADJUST_INT(0xFF000000);
	float HOVER_FONT_SIZE = ADJUST_FLOAT(64.0f);
	int HOVER_COLOR = ADJUST_INT(0xFF0403e8);

	UiWidgetId row = ui_push_container(h, "row", UI_WidgetFlag_Clickable, (UiSize){0}, (UiSize){0});
	UiWidgetInputs inputs = ui_widget_behavior(h, row);
	{
		int color = inputs.hovered ? HOVER_COLOR : COLOR;
		float font_size = inputs.hovered ? HOVER_FONT_SIZE : FONT_SIZE;
		ui_label_nt(h, "name", label, font_size, color);
	}
	ui_pop_parent(h);

	return inputs.clicked;
}

void mainmenu_render(struct Game *game)
{
	struct MainMenu *mainmenu = &game->mainmenu;

	UiHierarchy *h = &game->ui;

	UiWidgetId outer = ui_push_container(h, "vertical_container", 0, UI_SIZE_PERCENT(1.0f), UI_SIZE_PERCENT(1.0f));
	ui_widget_set_layout(h, outer, 1, 128.0f); // Y=1
	{

		mainmenu->local_pressed = _mainmenu_render_button(h, "PLAY LOCAL");
		mainmenu->network_pressed = _mainmenu_render_button(h, "HOST ONLINE");
		mainmenu->network_pressed = _mainmenu_render_button(h, "JOIN ONLINE");
		_mainmenu_render_button(h, "OPTIONS");
		_mainmenu_render_button(h, "QUIT");
	}
	ui_pop_parent(h);
}
