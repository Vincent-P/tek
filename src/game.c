#include "game.h"
#include "inputs.h"
#include <ufbx.h>

// -- Game
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

void game_simulate_frame(struct NonGameState *ngs, struct GameState *state, struct GameInputs inputs)
{
	game_state_update(state, inputs);
	
	// desync checks
	
	// Notify ggpo that we've moved forward exactly 1 frame.
	// ggpo_advance_frame

	// ggpoutil_perfmon_update
}

// -- Game state
void game_state_init(struct GameState *state)
{
	// initial game state
}

void game_non_state_init(struct NonGameState *state)
{
	Serializer s = serialize_begin_read_file("cooking/1801d6b015d7ce56");
	Serialize_SkeletalMeshWithAnimationsAsset(&s, &state->skeletal_mesh_with_animations);
	serialize_end_read_file(&s);

	ufbx_load_opts opts = { 0 }; // Optional, pass NULL for defaults
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.target_unit_meters = 1.0f;
	ufbx_scene *scene = ufbx_load_file("assets/tekken.fbx", &opts, NULL);
	assert(scene != NULL);
	state->scene = scene;
}

void game_state_update(struct GameState *state, struct GameInputs inputs)
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
	float speed = 10.0f;
	if ((inputs.player1 & GAME_INPUT_BACK) != 0) {
		state->player1.position[0] -= speed;
	}
	if ((inputs.player1 & GAME_INPUT_FORWARD) != 0) {
		state->player1.position[0] += speed;
	}

	if ((inputs.player2 & GAME_INPUT_BACK) != 0) {
		state->player2.position[0] -= speed;
	}
	if ((inputs.player2 & GAME_INPUT_FORWARD) != 0) {
		state->player2.position[0] += speed;
	}
	
	state->frame_number += 1;	
}

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

void game_state_render(struct GameState *state)
{
	ImDrawList* draw_list = ImGui_GetForegroundDrawList();
	ImVec2 pos = {0};
	pos.x = 100.0f + state->player1.position[0];
	pos.y = 100.0f;
	
	ImVec2 min = {0};
	min.x = pos.x - 10.0f;
	min.y = pos.y - 10.0f;
	ImVec2 max = {0};
	max.x = pos.x + 10.0f;
	max.y = pos.y + 10.0f;
	ImDrawList_AddRect(draw_list, min, max, IM_COL32(255, 0, 0, 255));

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
}

void game_non_state_render(struct NonGameState *state)
{
	Anim *anim_to_play = NULL;// state->skeletal_mesh_with_animations.animations + 1;

	static Float3 s_from = (Float3){1.0f, 0.0f, 0.0f};

	ImGui_DragFloat3Ex("from", &s_from.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
	

	Float4x4 proj = perspective_projection(50.0f, 1.0f, 0.1f, 100.0f, NULL);
	Float4x4 view = lookat_view(s_from, (Float3){0.0f, 0.0f, 0.0f});
	Float4x4 viewproj = float4x4_mul(proj, view);
	
	ImDrawList* draw_list = ImGui_GetForegroundDrawList();
	
	ufbx_pose *pose = state->scene->poses.data[0];

	#if 0
	for (size_t ibone = 0; ibone < pose->bone_poses.count; ++ibone) {
		ufbx_bone_pose *bone_pose = pose->bone_poses.data + ibone;

		ufbx_transform world_xform = ufbx_matrix_to_transform(&bone_pose->bone_to_world);


		Float3 translation;
		translation.x = world_xform.translation.x;
		translation.y = world_xform.translation.y;
		translation.z = world_xform.translation.z;
		
		Float3 p = float4x4_project_point(viewproj, translation);
		p.x = p.x * 0.5f + 0.5f;
		p.y = p.y * 0.5f + 0.5f;

		float imscale = 500.0f;
		float imoffset = 200.0f;
		ImVec2 min = {0};
		min.x = imscale * p.x - 10.0f + imoffset;
		min.y = imscale * p.y - 10.0f + imoffset;
		ImVec2 max = {0};
		max.x = imscale * p.x + 10.0f + imoffset;
		max.y = imscale * p.y + 10.0f + imoffset;
		ImDrawList_AddRect(draw_list, min, max, IM_COL32(255, 0, 0, 255));
	}
	#endif

	ufbx_anim_stack *stack = state->scene->anim_stacks.data[2];
	ufbx_anim *anim = stack->anim;

	float game_time = state->t;
	float duration = anim->time_end - anim->time_begin;
	while (game_time > duration) {
		game_time -= duration;
	}
		
	ufbx_bake_opts opts = {0};
	ufbx_baked_anim *bake = ufbx_bake_anim(state->scene, anim, &opts, NULL);
	assert(bake);

	
	ufbx_node *nodes[MAX_BONES_PER_MESH + 1] = {0};
	uint32_t parent_indices[MAX_BONES_PER_MESH + 1] = {0};
	ufbx_matrix node_to_parent[MAX_BONES_PER_MESH + 1] = {0};
	ufbx_matrix node_to_world[MAX_BONES_PER_MESH + 1] = {0};

	nodes[0] = state->scene->nodes.data[bake->nodes.data[0].typed_id]->parent;
	parent_indices[0] = 9999;
	node_to_parent[0] = nodes[0]->node_to_parent;
	node_to_world[0] = nodes[0]->node_to_world;
	
	for (size_t inode = 0; inode < bake->nodes.count; inode++) {
		ufbx_baked_node *baked_node = &bake->nodes.data[inode];
		ufbx_node *scene_node = state->scene->nodes.data[baked_node->typed_id];

		// assert that our parent is here
		uint32_t my_node_index = inode + 1;
		uint32_t iparent = 0;
		for (; iparent < my_node_index; ++iparent) {
			if (nodes[iparent] == scene_node->parent) {
				break;
			}
		}
		assert(iparent < my_node_index);

		ufbx_transform bone_pose;
		bone_pose.translation = ufbx_evaluate_baked_vec3(baked_node->translation_keys, game_time);
		bone_pose.rotation = ufbx_evaluate_baked_quat(baked_node->rotation_keys, game_time);
		bone_pose.scale = ufbx_evaluate_baked_vec3(baked_node->scale_keys, game_time);

		nodes[my_node_index] = scene_node;
		parent_indices[my_node_index] = iparent;
		node_to_parent[my_node_index] = ufbx_transform_to_matrix(&bone_pose);
	}
	
	for (size_t inode = 1; inode < bake->nodes.count + 1; inode++) {
		uint32_t iparent = parent_indices[inode];
		assert(iparent < bake->nodes.count + 1);
		
		node_to_world[inode] = ufbx_matrix_mul(&node_to_world[iparent], &node_to_parent[inode]);
	}

	for (size_t inode = 1; inode < bake->nodes.count + 1; inode++) {
		ufbx_vec3 up = ufbx_transform_position(&node_to_world[inode], (ufbx_vec3){});
		Float3 vertex;
		vertex.x = up.x;
		vertex.y = up.y;
		vertex.z = up.z;
			
			
		Float3 p = float4x4_project_point(viewproj, vertex);
		p.x = p.x * 0.5f + 0.5f;
		p.y = p.y * 0.5f + 0.5f;

		float imscale = 500.0f;
		float imoffset = 200.0f;
		ImVec2 min = {0};
		min.x = imscale * p.x - 10.0f + imoffset;
		min.y = imscale * p.y - 10.0f + imoffset;
		ImVec2 max = {0};
		max.x = imscale * p.x + 10.0f + imoffset;
		max.y = imscale * p.y + 10.0f + imoffset;
		ImDrawList_AddRect(draw_list, min, max, IM_COL32(255, 0, 0, 255));
	}


#if 0
	Float3 cube_vertices[] = {
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f},
		{0.0f, 1.0f, 0.0f},
		{0.0f, 1.0f, 1.0f},
		{1.0f, 0.0f, 0.0f},
		{1.0f, 0.0f, 1.0f},
		{1.0f, 1.0f, 0.0f},
		{1.0f, 1.0f, 1.0f},
	};
	for (uint32_t i = 0; i < ARRAY_LENGTH(cube_vertices); ++i) {		
		Float3 p = float4x4_project_point(viewproj, cube_vertices[i]);
		p.x = p.x * 0.5f + 0.5f;
		p.y = p.y * 0.5f + 0.5f;

		float imscale = 500.0f;
		float imoffset = 200.0f;
		ImVec2 min = {0};
		min.x = imscale * p.x - 10.0f + imoffset;
		min.y = imscale * p.y - 10.0f + imoffset;
		ImVec2 max = {0};
		max.x = imscale * p.x + 10.0f + imoffset;
		max.y = imscale * p.y + 10.0f + imoffset;
		ImDrawList_AddRect(draw_list, min, max, IM_COL32(255, 0, 0, 255));
	}
#endif
	
	if (anim_to_play != NULL) {
		for (uint32_t itrack = 0; itrack < anim_to_play->tracks_length; ++itrack) {
			AnimTrack *track = anim_to_play->tracks + itrack;
			// assert(track->translations.length == track->rotations.length);
			// assert(track->translations.length == track->scales.length);
			uint32_t iframe = state->frame_number % track->translations.length;

			Float3 translation = track->translations.data[iframe];
			Quat rotation = track->rotations.data[0];
			Float3 scale = track->scales.data[0];

			Float3 p = float4x4_project_point(viewproj, translation);
			p.x = p.x * 0.5f + 0.5f;
			p.y = p.y * 0.5f + 0.5f;

			float imscale = 500.0f;
			float imoffset = 200.0f;
			ImVec2 min = {0};
			min.x = imscale * p.x - 10.0f + imoffset;
			min.y = imscale * p.y - 10.0f + imoffset;
			ImVec2 max = {0};
			max.x = imscale * p.x + 10.0f + imoffset;
			max.y = imscale * p.y + 10.0f + imoffset;
			ImDrawList_AddRect(draw_list, min, max, IM_COL32(255, 0, 0, 255));
		}
	}
}
