#pragma once

struct AssetLibrary;
struct Renderer;
struct Inputs;

enum GameState
{
	GAME_STATE_MAIN_MENU = 0,
	GAME_STATE_LOCAL_BATTLE,
	GAME_STATE_NETWORK_BATTLE,
	GAME_STATE_COUNT,
};

struct Game
{
	enum GameState current_state; // TODO: stack? pushdown automata
	enum GameState requested_state;
	void *state_data;
	struct AssetLibrary *assets;
	struct Renderer *renderer;
	struct Inputs *inputs;
};

struct GameUpdateContext
{
	struct Inputs inputs;
	uint64_t current_time;
	uint64_t previous_frame_time;
	uint64_t f;
};

enum GameUpdateAction
{
	GAME_UPDATE_ACTION_CONTINUE = 0,
	GAME_UPDATE_ACTION_TRANSITION,
};

struct GameUpdateResult
{
	enum GameUpdateAction action;
	enum GameState transition_state;
};

typedef void (*StateInitFn)(void **state_data, struct Game const *game);
typedef void (*StateTermFn)(void **state_data);
typedef struct GameUpdateResult (*StateUpdateFn)(void **state_data, struct GameUpdateContext const *ctx);
typedef void (*StateRenderFn)(void **state_data);

void mainmenu_init(void **state_data, struct Game const *game);
void mainmenu_term(void **state_data);
struct GameUpdateResult mainmenu_update(void **state_data, struct GameUpdateContext const *ctx);
void mainmenu_render(void **state_data);

void local_battle_init(void **state_data, struct Game const *game);
void local_battle_term(void **state_data);
struct GameUpdateResult local_battle_update(void **state_data, struct GameUpdateContext const *ctx);
void local_battle_render(void **state_data);

void network_battle_init(void **state_data, struct Game const *game);
void network_battle_term(void **state_data);
struct GameUpdateResult network_battle_update(void **state_data, struct GameUpdateContext const *ctx);
void network_battle_render(void **state_data);

StateInitFn const STATE_INIT_FUNCTIONS[GAME_STATE_COUNT] = {
	mainmenu_init,
	local_battle_init,
	network_battle_init,
};

StateTermFn const STATE_TERM_FUNCTIONS[GAME_STATE_COUNT] = {
	mainmenu_term,
	local_battle_term,
	network_battle_term,
};

StateUpdateFn const STATE_UPDATE_FUNCTIONS[GAME_STATE_COUNT] = {
	mainmenu_update,
	local_battle_update,
	network_battle_update,
};

StateRenderFn const STATE_RENDER_FUNCTIONS[GAME_STATE_COUNT] = {
	mainmenu_render,
	local_battle_render,
	network_battle_render,
};

void game_init(struct Game *game);
void game_update(struct Game *game, struct GameUpdateContext const* ctx);
void game_render(struct Game *game);
