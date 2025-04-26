#include "game.h"
#include "inputs.h"
#include "debugdraw.h"
#include "renderer.h"
#include "editor.h"

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
void game_state_update(struct NonGameState *nonstate, struct GameState *state, struct GameInputs inputs);

void game_simulate_frame(struct NonGameState *ngs, struct GameState *state, struct GameInputs inputs)
{
	TracyCZoneN(f, "SimulateFrame", true);
	debug_draw_reset();

	game_state_update(ngs, state, inputs);

	// desync checks

	// Notify ggpo that we've moved forward exactly 1 frame.
	// ggpo_advance_frame

	// ggpoutil_perfmon_update
	TracyCZoneEnd(f);
}

// -- Game state

void game_state_init(struct GameState *state, struct NonGameState *nonstate, Renderer *renderer)
{
	// Initial game state
	state->player1_entity.spatial.world_transform.cols[0].z = -1.0f;
	state->player1_entity.spatial.world_transform.cols[1].y = 1.0f;
	state->player1_entity.spatial.world_transform.cols[2].x = 1.0f;
	state->player1_entity.spatial.world_transform.cols[3].x = -1.0f;
	state->player2_entity.spatial.world_transform.cols[0].x = 1.0f;
	state->player2_entity.spatial.world_transform.cols[1].y = 1.0f;
	state->player2_entity.spatial.world_transform.cols[2].z = 1.0f;
	state->player2_entity.spatial.world_transform.cols[3].x = 1.0f;
	state->player1_entity.anim_skeleton.anim_skeleton_id = 3189085727;
	state->player2_entity.anim_skeleton.anim_skeleton_id = 3189085727;
	state->player1_entity.animation.animation_id = 3108588149;
	state->player2_entity.animation.animation_id = 3108588149;

	// initial non game state
	nonstate->renderer = renderer;

	// Init players
	SkeletalMeshAsset const *skeletal_mesh = asset_library_get_skeletal_mesh(nonstate->assets, 1812931262);
	AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(nonstate->assets, 3189085727);
	// create an instance to hold the render pose
	skeletal_mesh_create_instance(skeletal_mesh, &nonstate->p1_mesh_instance, anim_skeleton);
	skeletal_mesh_create_instance(skeletal_mesh, &nonstate->p2_mesh_instance, anim_skeleton);
	// create render instances
	struct SkeletalMeshInstanceData render_instance_data = {0};
	render_instance_data.mesh = skeletal_mesh;
	render_instance_data.dynamic_data_mesh = &nonstate->p1_mesh_instance;
	render_instance_data.dynamic_data_spatial = &state->player1_entity.spatial;
	renderer_register_skeletal_mesh_instance(renderer, render_instance_data);	
	render_instance_data.dynamic_data_mesh = &nonstate->p2_mesh_instance;
	render_instance_data.dynamic_data_spatial = &state->player2_entity.spatial;
	renderer_register_skeletal_mesh_instance(renderer, render_instance_data);
	
	// Init camera
	nonstate->camera.position.y = 0.8f;
	nonstate->camera.position.z = 6.0f;
	nonstate->camera.lookat.y = 0.8f;
	nonstate->camera.vertical_fov = 40.0f;
}

void game_state_update(struct NonGameState *nonstate, struct GameState *state, struct GameInputs inputs)
{
	TracyCZoneN(f, "StateUpdate", true);
	
	// register input in the input buffer
	struct TekPlayerComponent *p1 = &state->player1_entity.tek;
	if (inputs.player1 != p1->input_buffer[p1->current_input_index % INPUT_BUFFER_SIZE]) {
		p1->current_input_index += 1;
		uint32_t input_index = p1->current_input_index % INPUT_BUFFER_SIZE;
		p1->input_buffer[input_index] = inputs.player1;
		p1->input_buffer_frame_start[input_index] = state->frame_number;
	}

	// match inputs

	// gameplay update
	struct SpatialComponent *p1_root = &state->player1_entity.spatial;
	struct SpatialComponent *p2_root = &state->player2_entity.spatial;
	float speed = 0.1f;
	if ((inputs.player1 & GAME_INPUT_BACK) != 0) {
		p1_root->world_transform.cols[3].x -= speed;
	}
	if ((inputs.player1 & GAME_INPUT_FORWARD) != 0) {
		p1_root->world_transform.cols[3].x += speed;
	}
	if ((inputs.player2 & GAME_INPUT_BACK) != 0) {
		p2_root->world_transform.cols[3].x -= speed;
	}
	if ((inputs.player2 & GAME_INPUT_FORWARD) != 0) {
		p2_root->world_transform.cols[3].x += speed;
	}

	state->frame_number += 1;

	// Evaluate anims
	Float3 average_pos = float3_add(p1_root->world_transform.cols[3], p2_root->world_transform.cols[3]);
	average_pos.x *= 0.5f;
	average_pos.y *= 0.5f;
	average_pos.z *= 0.5f;
	nonstate->camera.lookat = average_pos;

	struct AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(nonstate->assets,
									     state->player1_entity.anim_skeleton.anim_skeleton_id);
	struct Animation const *animation = asset_library_get_animation(nonstate->assets, state->player1_entity.animation.animation_id);
	if (animation != NULL) {
		anim_evaluate_animation(anim_skeleton, animation, &nonstate->p1_pose, (float)state->frame_number);
		anim_pose_compute_global_transforms(anim_skeleton, &nonstate->p1_pose);
		skeletal_mesh_apply_pose(&nonstate->p1_mesh_instance, &nonstate->p1_pose);

		// debug draw animated pose
		for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
			Float3 p;
			p.x = F34(nonstate->p1_pose.global_transforms[ibone], 0, 3);
			p.y = F34(nonstate->p1_pose.global_transforms[ibone], 1, 3);
			p.z = F34(nonstate->p1_pose.global_transforms[ibone], 2, 3);
			p = float3_add(p, state->player1_entity.spatial.world_transform.cols[3]);
			debug_draw_point(p);
		}
	}

	// debug draw skeleton at P2
	for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
		Float3 p;
		p.x = F34(anim_skeleton->bones_global_transforms[ibone], 0, 3);
		p.y = F34(anim_skeleton->bones_global_transforms[ibone], 1, 3);
		p.z = F34(anim_skeleton->bones_global_transforms[ibone], 2, 3);
		p = float3_add(p, state->player2_entity.spatial.world_transform.cols[3]);
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
	
	TracyCZoneEnd(f);
}

// -- Render

void game_render(struct NonGameState *nonstate, struct GameState *state)
{
	TracyCZoneN(f, "GameRender", true);
	
	struct TekPlayerComponent const *p1 = &state->player1_entity.tek;
	if (ImGui_Begin("Player 1 inputs", NULL, 0)) {
		if (ImGui_BeginTable("inputs", 2, ImGuiTableFlags_Borders)) {
			uint32_t input_last_frame = state->frame_number;
			for (uint32_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
				uint32_t input_index = (p1->current_input_index + (INPUT_BUFFER_SIZE - i)) % INPUT_BUFFER_SIZE;
				enum GameInputBits input = p1->input_buffer[input_index];
				uint32_t input_duration = input_last_frame - p1->input_buffer_frame_start[input_index];
				input_last_frame = p1->input_buffer_frame_start[input_index];

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
	ed_display_player_entity("Player 1", &state->player1_entity);
	ed_display_player_entity("Player 2", &state->player2_entity);

	if (ImGui_Begin("Game", NULL, 0)) {
		ImGui_DragFloat3Ex("camera position", &nonstate->camera.position.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat3Ex("camera lookat", &nonstate->camera.lookat.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat("camera fov", &nonstate->camera.vertical_fov);
	}
	ImGui_End();

	TracyCZoneEnd(f);
}
