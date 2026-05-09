#pragma once
struct Game;
struct GameUpdateContext;


struct MainMenu
{
	bool local_pressed;
	bool network_pressed;
};


void mainmenu_init(struct MainMenu *mainmenu);
void mainmenu_term(struct MainMenu *mainmenu);
bool mainmenu_update(struct Game *game, struct GameUpdateContext const *ctx);
void mainmenu_render(struct MainMenu *mainmenu);
