#include "game.h"
#include "inputs.h"
#include "debugdraw.h"
#include "renderer.h"

// -- Game Inputs

static const char* _get_motion_label(enum GameInputBits bits)
{
	uint8_t MOTION_MASK = 0x0f;
	const char *MOTIONS_LABELS[] = {
		" ",
		"b",
		"u",
		"bu",
		"f",
		"bf",
		"uf",
		"buf",
		"d",
		"db",
		"du",
		"dbu",
		"df",
		"dbf",
		"duf",
		"dbuf",
	};

	uint8_t dirs = bits & MOTION_MASK;
	assert(dirs < ARRAY_LENGTH(MOTIONS_LABELS));
	return MOTIONS_LABELS[dirs];
}

static const char* _get_action_label(enum GameInputBits bits)
{
	uint8_t ACTION_MASK = 0xf0;
	const char *ACTIONS_LABELS[] = {
		" ",
		"LP",
		"RP",
		"LP+RP",
		"LK",
		"LP+LK",
		"RP+LK",
		"LP+RP+LK",
		"RK",
		"LP+RK",
		"RP+RK",
		"LP+RP+RK",
		"LK+RK",
		"LP+LK+RK",
		"RP+LK+RK",
		"LP+RP+LK+RK",
	};

	uint8_t actions = (bits & ACTION_MASK) >> 4;
	assert(actions < ARRAY_LENGTH(ACTIONS_LABELS));
	return ACTIONS_LABELS[actions];
}

struct GameInputs game_read_input(struct Inputs *inputs)
{
	struct GameInputs input = {0};

	if (inputs->buttons_is_pressed[InputButtons_W]) {
		input.player1 |= GAME_INPUT_UP;
	}
	if (inputs->buttons_is_pressed[InputButtons_A]) {
		input.player1 |= GAME_INPUT_BACK;
	}
	if (inputs->buttons_is_pressed[InputButtons_S]) {
		input.player1 |= GAME_INPUT_DOWN;
	}
	if (inputs->buttons_is_pressed[InputButtons_D]) {
		input.player1 |= GAME_INPUT_FORWARD;
	}
	if (inputs->buttons_is_pressed[InputButtons_U]) {
		input.player1 |= GAME_INPUT_LPUNCH;
	}
	if (inputs->buttons_is_pressed[InputButtons_I]) {
		input.player1 |= GAME_INPUT_RPUNCH;
	}
	if (inputs->buttons_is_pressed[InputButtons_J]) {
		input.player1 |= GAME_INPUT_LKICK;
	}
	if (inputs->buttons_is_pressed[InputButtons_K]) {
		input.player1 |= GAME_INPUT_RKICK;
	}
	return input;
}

// -- Game

void game_simulate_frame(struct NonGameState *ngs, struct GameState *state, struct GameInputs inputs)
{
	debug_draw_reset();

	game_state_update(ngs, state, inputs);

	// desync checks

	// Notify ggpo that we've moved forward exactly 1 frame.
	// ggpo_advance_frame

	// ggpoutil_perfmon_update
}

// -- Game state

void game_state_init(struct GameState *state)
{
	// initial game state
	state->player1.position.x = -1.0f;
	state->player2.position.x =  1.0f;
}

void game_non_state_init(struct NonGameState *state, Renderer *renderer)
{
	state->renderer = renderer;

	state->camera.position.y = 0.8f;
	state->camera.position.z = 6.0f;
	state->camera.lookat.y = 0.8f;
	state->camera.vertical_fov = 40.0f;

	// Load skeletal mesh
	Serializer s = serialize_begin_read_file("cooking/28ad6a937f9e6a93");
	Serialize_SkeletalMeshWithAnimationsAsset(&s, &state->skeletal_mesh_with_animations);
	serialize_end_read_file(&s);
	skeletal_mesh_init(&state->skeletal_mesh_with_animations.skeletal_mesh,
			   &state->p1_mesh_instance,
			   &state->skeletal_mesh_with_animations.anim_skeleton);
	renderer_create_skeletal_mesh(renderer, &state->skeletal_mesh_with_animations.skeletal_mesh, 0);
}

void game_state_update(struct NonGameState *nonstate, struct GameState *state, struct GameInputs inputs)
{
	// register input in the input buffer
	if (inputs.player1 != state->player1.input_buffer[state->player1.current_input_index % INPUT_BUFFER_SIZE]) {
		state->player1.current_input_index += 1;
		uint32_t input_index = state->player1.current_input_index % INPUT_BUFFER_SIZE;
		state->player1.input_buffer[input_index] = inputs.player1;
		state->player1.input_buffer_frame_start[input_index] = state->frame_number;
	}

	// match inputs

	// gameplay update
	float speed = 0.1f;
	if ((inputs.player1 & GAME_INPUT_BACK) != 0) {
		state->player1.position.x -= speed;
	}
	if ((inputs.player1 & GAME_INPUT_FORWARD) != 0) {
		state->player1.position.x += speed;
	}

	if ((inputs.player2 & GAME_INPUT_BACK) != 0) {
		state->player2.position.x -= speed;
	}
	if ((inputs.player2 & GAME_INPUT_FORWARD) != 0) {
		state->player2.position.x += speed;
	}

	state->frame_number += 1;

	// Evaluate anims
	static uint32_t ianim = 0;

	struct SkeletalMeshWithAnimationsAsset *asset = &nonstate->skeletal_mesh_with_animations;
	struct AnimSkeleton *anim_skeleton = &asset->anim_skeleton;
	struct Animation *animation = ianim < asset->animations_length ? asset->animations + ianim : NULL;
	if (animation != NULL) {
		nonstate->p1_pose.skeleton_id = anim_skeleton->id;
		anim_evaluate_animation(anim_skeleton, animation, &nonstate->p1_pose, (float)state->frame_number);
		anim_pose_compute_global_transforms(anim_skeleton, &nonstate->p1_pose);
		skeletal_mesh_apply_pose(&nonstate->p1_mesh_instance, &nonstate->p1_pose);

		// debug draw animated pose
		for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
			Float3 p;
			p.x = F34(nonstate->p1_pose.global_transforms[ibone], 0, 3);
			p.y = F34(nonstate->p1_pose.global_transforms[ibone], 1, 3);
			p.z = F34(nonstate->p1_pose.global_transforms[ibone], 2, 3);
			debug_draw_point(p);
		}
	}

	// debug draw skeleton at P2
	for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
		Float3 p;
		p.x = F34(anim_skeleton->bones_global_transforms[ibone], 0, 3);
		p.y = F34(anim_skeleton->bones_global_transforms[ibone], 1, 3);
		p.z = F34(anim_skeleton->bones_global_transforms[ibone], 2, 3);
		p = float3_add(p, state->player2.position);
		debug_draw_point(p);
	}
	// debug draw grid
	float width = 24.0f;
	for (float i = 1.0f; i <= width; i += 1.0f) {
		debug_draw_line((Float3){i, 0.0f, -width}, (Float3){i, 0.0f, width}, DD_WHITE & DD_HALF_ALPHA);
		debug_draw_line((Float3){-i, 0.0f, -width}, (Float3){-i, 0.0f, width}, DD_WHITE & DD_HALF_ALPHA);
		debug_draw_line((Float3){-width, 0.0f, i}, (Float3){width, 0.0f, i}, DD_WHITE & DD_HALF_ALPHA);
		debug_draw_line((Float3){-width, 0.0f, -i}, (Float3){width, 0.0f, -i}, DD_WHITE & DD_HALF_ALPHA);
	}
	debug_draw_line((Float3){0.0f, 0.0f, -width}, (Float3){0.0f, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
	debug_draw_line((Float3){0.0f, 0.0f, 0.0f}, (Float3){0.0f, 0.0f, width}, DD_BLUE);
	debug_draw_line((Float3){-width, 0.0f, 0.0f}, (Float3){0, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
	debug_draw_line((Float3){0, 0.0f, 0.0f}, (Float3){width, 0.0f, 0.0f}, DD_RED);
	width = 2.4f;
	for (float i = 0.1f; i <= width; i += 0.1f) {
		debug_draw_line((Float3){i, 0.0f, -width}, (Float3){i, 0.0f, width}, DD_WHITE & DD_QUARTER_ALPHA);
		debug_draw_line((Float3){-i, 0.0f, -width}, (Float3){-i, 0.0f, width}, DD_WHITE & DD_QUARTER_ALPHA);
		debug_draw_line((Float3){-width, 0.0f, i}, (Float3){width, 0.0f, i}, DD_WHITE & DD_QUARTER_ALPHA);
		debug_draw_line((Float3){-width, 0.0f, -i}, (Float3){width, 0.0f, -i}, DD_WHITE & DD_QUARTER_ALPHA);
	}
}

// -- Render

void game_render(struct NonGameState *nonstate, struct GameState *state)
{
	if (ImGui_Begin("Player 1 inputs", NULL, 0)) {
		if (ImGui_BeginTable("inputs", 2, ImGuiTableFlags_Borders)) {
			uint32_t input_last_frame = state->frame_number;
			for (uint32_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
				uint32_t input_index = (state->player1.current_input_index + (INPUT_BUFFER_SIZE - i)) % INPUT_BUFFER_SIZE;
				enum GameInputBits input = state->player1.input_buffer[input_index];
				uint32_t input_duration = input_last_frame - state->player1.input_buffer_frame_start[input_index];
				input_last_frame = state->player1.input_buffer_frame_start[input_index];

				ImGui_TableNextRow();
				ImGui_TableSetColumnIndex(0);
				char input_label[32] = {0};
				sprintf(input_label, "%s %s", _get_motion_label(input), _get_action_label(input));
				ImGui_TextUnformatted(input_label);

				ImGui_TableSetColumnIndex(1);
				char duration_label[32] = {0};
				sprintf(duration_label, "%u", input_duration);
				ImGui_TextUnformatted(duration_label);
			}
			ImGui_EndTable();
		}
	}
	ImGui_End();

	if (ImGui_Begin("Game", NULL, 0)) {
		ImGui_DragFloat3Ex("camera position", &nonstate->camera.position.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat3Ex("camera lookat", &nonstate->camera.lookat.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat("camera fov", &nonstate->camera.vertical_fov);
	}
	ImGui_End();

	renderer_submit_skeletal_instance(nonstate->renderer, &nonstate->p1_mesh_instance);
}
