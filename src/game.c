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

void game_state_init_player(struct PlayerEntity *p, struct PlayerNonEntity *pn, struct NonGameState *nonstate)
{
	struct tek_Character *c = tek_characters + p->tek.character_id;
	p->anim_skeleton.anim_skeleton_id = c->anim_skeleton_id;
	p->animation.animation_id = 3108588149; // idle

	SkeletalMeshAsset const *skeletal_mesh = asset_library_get_skeletal_mesh(nonstate->assets, c->skeletal_mesh_id);
	AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(nonstate->assets, c->anim_skeleton_id);

	// create an instance to hold the render pose
	skeletal_mesh_create_instance(skeletal_mesh, &pn->mesh_instance, anim_skeleton);
	// create render instances
	struct SkeletalMeshInstanceData render_instance_data = {0};
	render_instance_data.mesh = skeletal_mesh;
	render_instance_data.dynamic_data_mesh = &pn->mesh_instance;
	render_instance_data.dynamic_data_spatial = &p->spatial;
	renderer_register_skeletal_mesh_instance(nonstate->renderer, render_instance_data);	
}

void game_state_init(struct GameState *state, struct NonGameState *nonstate, Renderer *renderer)
{
	// Initial game state
	Float3 p1_initial_pos = (Float3){-1.0f, 0.f, 0.f};
	Float3 p2_initial_pos = (Float3){ 1.0f, 0.f, 0.f};
	spatial_component_set_position(&state->p1_entity.spatial, p1_initial_pos);
	state->p1_entity.spatial.world_transform.cols[0] = (Float3){1.0f, 0.0f, 0.0f};
	state->p1_entity.spatial.world_transform.cols[1] = (Float3){0.0f, 1.0f, 0.0f};
	state->p1_entity.spatial.world_transform.cols[2] = (Float3){0.0f, 0.0f, 1.0f};
	spatial_component_target(&state->p1_entity.spatial, (Float3){0});//p2_initial_pos);
	spatial_component_set_position(&state->p2_entity.spatial, p2_initial_pos);
	state->p2_entity.spatial.world_transform.cols[0] = (Float3){1.0f, 0.0f, 0.0f};
	state->p2_entity.spatial.world_transform.cols[1] = (Float3){0.0f, 1.0f, 0.0f};
	state->p2_entity.spatial.world_transform.cols[2] = (Float3){0.0f, 0.0f, 1.0f};
	spatial_component_target(&state->p2_entity.spatial, (Float3){0});//p1_initial_pos);

	// initial non game state
	nonstate->renderer = renderer;
	// Init players
	game_state_init_player(&state->p1_entity, &nonstate->p1_nonentity, nonstate);
	game_state_init_player(&state->p2_entity, &nonstate->p2_nonentity, nonstate);
	// Init camera
	nonstate->camera_distance = 3.0f;
	nonstate->camera.position = (Float3){1.0f, -5.0f, 1.0f};
	nonstate->camera.vertical_fov = 40.0f;
}

void game_state_update(struct NonGameState *nonstate, struct GameState *state, struct GameInputs inputs)
{
	TracyCZoneN(f, "StateUpdate", true);
	
	// register input in the input buffer
	struct PlayerEntity *p1 = &state->p1_entity;
	struct tek_Character *c1 = tek_characters + p1->tek.character_id;
	if (inputs.player1 != p1->tek.input_buffer[p1->tek.current_input_index % INPUT_BUFFER_SIZE]) {
		p1->tek.current_input_index += 1;
		uint32_t input_index = p1->tek.current_input_index % INPUT_BUFFER_SIZE;
		p1->tek.input_buffer[input_index] = inputs.player1;
		p1->tek.input_buffer_frame_start[input_index] = state->frame_number;
	}
	// match inputs
	struct tek_Stance *current_stance = &c1->stances[0];
	struct tek_Move *move_request = NULL;
	for (uint32_t imove = 0; imove < current_stance->moves_length; ++imove) {
		struct tek_Move *move = &current_stance->moves[imove];
		if (move->action_input == inputs.player1) {
			move_request = move;
			break;
		}
	}

	// -- gameplay update
	
	// move request
	if (move_request != NULL) {
		// A player can only do a move if they are not in active, recovery, or startup
		if (p1->tek.action_state.type == TEK_ACTION_STATE_NONE) {
			printf("p1 wants to do move %u\n", move_request->id);
			printf("play anim %u\n", move_request->animation_id);

			p1->animation.animation_id = move_request->animation_id;
			p1->animation.frame = 0;
			
			p1->tek.action_state.type = TEK_ACTION_STATE_STARTUP;
			p1->tek.action_state.end = state->frame_number + move_request->startup;
		}
	}

	// update player state
	
	if (p1->tek.action_state.type != TEK_ACTION_STATE_NONE) {
		struct tek_ActionState action_state = p1->tek.action_state;
		if (action_state.type == TEK_ACTION_STATE_STARTUP) {
			if (state->frame_number >= action_state.end) {
				
			}
		} else if (action_state.type == TEK_ACTION_STATE_ACTIVE) {
			
		} else if (action_state.type == TEK_ACTION_STATE_RECOVERY) {
		}
	}
	
	


	// update transforms
	struct SpatialComponent *p1_root = &state->p1_entity.spatial;
	struct SpatialComponent *p2_root = &state->p2_entity.spatial;
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
	

	// Camera follow
	if (nonstate->camera_focus != 0)
	{
		Float3 target_pos = float3_add(p1_root->world_transform.cols[3], p2_root->world_transform.cols[3]);
		target_pos.x *= 0.5f;
		target_pos.y *= 0.5f;
		target_pos.z *= 0.5f;
		Float3 camera_pos = nonstate->camera.position;
		float target_distance = float3_distance(target_pos, camera_pos);
		
		float ratio = 9.0f / 16.0f;
		float fov_rad = nonstate->camera.vertical_fov * 2.0f * 3.14f / 360.0f;
		float width_from_dist = ratio * 2.0f * tanf(fov_rad / 2.0f);

		float target_width = width_from_dist * target_distance;
		float ideal_width = float3_distance(p1_root->world_transform.cols[3], p2_root->world_transform.cols[3]);

		float ideal_distance = target_distance;
		//if (target_width - ideal_width > 0.1f) {
			ideal_distance = ideal_width / width_from_dist;
		//}

			Float3 camera_dir = p1_root->world_transform.cols[0];
			if (nonstate->camera_focus == 2) {
				camera_dir = p2_root->world_transform.cols[0];
			}
		nonstate->camera_distance = ideal_distance;
		nonstate->camera.position = float3_add(target_pos, float3_mul_scalar(camera_dir, ideal_distance));
		nonstate->camera.lookat = target_pos;
		
		nonstate->camera.position.z += 1.0f;
		nonstate->camera.lookat.z += 1.0f;
	}

	// Evaluate anims
	struct PlayerNonEntity *np1 = &nonstate->p1_nonentity;
	struct AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(nonstate->assets, p1->anim_skeleton.anim_skeleton_id);
	struct Animation const *animation = asset_library_get_animation(nonstate->assets, p1->animation.animation_id);
	if (animation != NULL) {
		bool has_ended = anim_evaluate_animation(anim_skeleton, animation, &np1->pose, p1->animation.frame);
		if (has_ended) {
			// HACK: reset to none state
			p1->tek.action_state.type = TEK_ACTION_STATE_NONE;

			
			p1->animation.animation_id = 3108588149;
			p1->animation.frame = 0;
		}
		// update render skeleton
		anim_pose_compute_global_transforms(anim_skeleton, &np1->pose);
		skeletal_mesh_apply_pose(&np1->mesh_instance, &np1->pose);
		// use root motion
		Float3 root_translation = float3x4_transform_direction(p1->spatial.world_transform, np1->pose.root_motion_delta_translation);
		p1->spatial.world_transform.cols[3] = float3_add(p1->spatial.world_transform.cols[3], root_translation);
	}

	// Update anims
	p1->animation.frame += 1;

	// update hurtboxes positions
	{
		// Apply the bone pose to the hurtboxes
		for (uint32_t ihurtbox = 0; ihurtbox < c1->hurtboxes_length; ++ihurtbox) {
			uint32_t hurtbox_bone_id = c1->hurtboxes_bone_id[ihurtbox];
			for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ++ibone){
				if (anim_skeleton->bones_identifier[ibone] == hurtbox_bone_id) {
					np1->hurtboxes_position[ihurtbox] = float3x4_transform_point(
										       p1->spatial.world_transform,
										       np1->pose.global_transforms[ibone].cols[3]);
					break;
				}
			}
		}
	}
	
	// debug draw animated pose
	for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
		Float3 p;
		p.x = F34(np1->pose.global_transforms[ibone], 0, 3);
		p.y = F34(np1->pose.global_transforms[ibone], 1, 3);
		p.z = F34(np1->pose.global_transforms[ibone], 2, 3);
		p = float3x4_transform_point(p1->spatial.world_transform, p);
		debug_draw_point(p);
	}
	// debug draw hurtboxes cylinders
	for (uint32_t ihurtbox = 0; ihurtbox < c1->hurtboxes_length; ++ihurtbox) {
		Float3 center = np1->hurtboxes_position[ihurtbox];
		float radius = c1->hurtboxes_radius[ihurtbox];
		float height = c1->hurtboxes_height[ihurtbox];
		debug_draw_cylinder(center, radius, height, DD_GREEN);
	}
	// debug draw skeleton at P2
	for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
		Float3 p;
		p.x = F34(anim_skeleton->bones_global_transforms[ibone], 0, 3);
		p.y = F34(anim_skeleton->bones_global_transforms[ibone], 1, 3);
		p.z = F34(anim_skeleton->bones_global_transforms[ibone], 2, 3);
		p = float3x4_transform_point(state->p2_entity.spatial.world_transform, p);
		debug_draw_point(p);
	}
	// debug draw grid
	float width = 24.0f;
	if (1) {
		for (float i = 1.0f; i <= width; i += 1.0f) {
			debug_draw_line((Float3){i, -width, 0.0f}, (Float3){i, width, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
			debug_draw_line((Float3){-i, -width, 0.0f}, (Float3){-i, width, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
			debug_draw_line((Float3){-width, i, 0.0f}, (Float3){width, i, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
			debug_draw_line((Float3){-width, -i, 0.0f}, (Float3){width, -i, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
		}
	}
	debug_draw_line((Float3){-width, 0.0f, 0.0f}, (Float3){0, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
	debug_draw_line((Float3){0, 0.0f, 0.0f}, (Float3){width, 0.0f, 0.0f}, DD_RED);
	debug_draw_line((Float3){0.0f, -width, 0.0f}, (Float3){0, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
	debug_draw_line((Float3){0, 0.0f, 0.0f}, (Float3){0.0f, width, 0.0f}, DD_GREEN);
	debug_draw_line((Float3){0.0f, 0.0f, -width}, (Float3){0.0f, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
	debug_draw_line((Float3){0.0f, 0.0f, 0.0f}, (Float3){0.0f, 0.0f, width}, DD_BLUE);
	if (1) {
		width = 2.4f;
		for (float i = 0.1f; i <= width; i += 0.1f) {
			debug_draw_line((Float3){i, -width, 0.0f}, (Float3){i, width, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
			debug_draw_line((Float3){-i, -width, 0.0f}, (Float3){-i, width, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
			debug_draw_line((Float3){-width, i, 0.0f}, (Float3){width, i, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
			debug_draw_line((Float3){-width, -i, 0.0f}, (Float3){width, -i, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
		}
	}
	
	// draw local axis for P1
	Float3 origin = {0};
	Float3 x_axis = (Float3){0.1f, 0.0f, 0.0f};
	Float3 y_axis = (Float3){0.0f, 0.1f, 0.0f};
	Float3 z_axis = (Float3){0.0f, 0.0f, 0.1f};
	Float3 p1_o = float3x4_transform_point(p1_root->world_transform, origin);
	Float3 p1_x = float3x4_transform_point(p1_root->world_transform, x_axis);
	Float3 p1_y = float3x4_transform_point(p1_root->world_transform, y_axis);
	Float3 p1_z = float3x4_transform_point(p1_root->world_transform, z_axis);
	debug_draw_line(p1_o, p1_x, DD_RED);
	debug_draw_line(p1_o, p1_y, DD_GREEN);
	debug_draw_line(p1_o, p1_z, DD_BLUE);
	// draw local axis for P2
	Float3 p2_o = float3x4_transform_point(p2_root->world_transform, origin);
	Float3 p2_x = float3x4_transform_point(p2_root->world_transform, x_axis);
	Float3 p2_y = float3x4_transform_point(p2_root->world_transform, y_axis);
	Float3 p2_z = float3x4_transform_point(p2_root->world_transform, z_axis);
	debug_draw_line(p2_o, p2_x, DD_RED);
	debug_draw_line(p2_o, p2_y, DD_GREEN);
	debug_draw_line(p2_o, p2_z, DD_BLUE);
	
	TracyCZoneEnd(f);
}

// -- Render

void game_render(struct NonGameState *nonstate, struct GameState *state)
{
	TracyCZoneN(f, "GameRender", true);
	
	struct TekPlayerComponent const *p1 = &state->p1_entity.tek;
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
	ed_display_player_entity("Player 1", &state->p1_entity);
	ed_display_player_entity("Player 2", &state->p2_entity);

	if (ImGui_Begin("Game", NULL, 0)) {
		ImGui_DragFloat3Ex("camera position", &nonstate->camera.position.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat3Ex("camera lookat", &nonstate->camera.lookat.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat("camera fov", &nonstate->camera.vertical_fov);
		ImGui_DragInt("camera focus", &nonstate->camera_focus);
	}
	ImGui_End();

	TracyCZoneEnd(f);
}
