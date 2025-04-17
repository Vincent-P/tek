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
	Serializer s = serialize_begin_read_file("cooking/8e4d0cfae7ddad95");
	Serialize_SkeletalMeshWithAnimationsAsset(&s, &state->skeletal_mesh_with_animations);
	serialize_end_read_file(&s);
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
	static Float3 s_from = (Float3){1.0f, 0.0f, 0.0f};
	ImGui_DragFloat3Ex("from", &s_from.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
	
	Float4x4 proj = perspective_projection(50.0f, 1.0f, 0.1f, 100.0f, NULL);
	Float4x4 view = lookat_view(s_from, (Float3){0.0f, 0.0f, 0.0f});
	Float4x4 viewproj = float4x4_mul(proj, view);

	static int32_t ianim = 0;
	ImGui_DragInt("anim", &ianim);
	
	ImDrawList* draw_list = ImGui_GetForegroundDrawList();
	
	struct SkeletalMeshWithAnimationsAsset *asset = &state->skeletal_mesh_with_animations;
	struct AnimSkeleton *anim_skeleton = &asset->anim_skeleton;
	struct Animation *animation = ianim < asset->animations_length ? asset->animations + ianim : NULL;

	struct AnimPose new_pose = {0};
	if (animation != NULL) {
		new_pose.skeleton_id = anim_skeleton->id;
		anim_evaluate_animation(animation, &new_pose, (float)state->frame_number);

		Float3x4 root_world_transform = {0};
		F34(root_world_transform, 0, 0) = 1.0f;
		F34(root_world_transform, 1, 1) = 1.0f;
		F34(root_world_transform, 2, 2) = 1.0f;
		anim_pose_compute_global_transforms(anim_skeleton, &new_pose, root_world_transform);
	
		for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
			Float3 vertex;
			vertex.x = F34(new_pose.global_transforms[ibone], 0, 3);
			vertex.y = F34(new_pose.global_transforms[ibone], 1, 3);
			vertex.z = F34(new_pose.global_transforms[ibone], 2, 3);
		
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
	}
}
