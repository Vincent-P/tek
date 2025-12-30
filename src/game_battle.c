#include "game_battle.h"
#include "inputs.h"
#include "debugdraw.h"
#include "renderer.h"
#include "editor.h"

struct CancelContext
{
	uint32_t current_frame;
	uint32_t animation_frame; // current frame of the animation
	uint32_t animation_length; // number of frames in the animation
};


// -- Battle Inputs
static const char* _get_motion_label(enum BattleInputBits bits)
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

static const char* _get_action_label(enum BattleInputBits bits)
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

BattleInput battle_clean_socd_input(BattleInput input)
{
	bool socd_bf = ((input & BATTLE_INPUT_FORWARD) != 0) &&  ((input & BATTLE_INPUT_BACK) != 0);
	if (socd_bf) {
		// clean bits
		input ^= BATTLE_INPUT_FORWARD;
		input ^= BATTLE_INPUT_BACK;
	}
	bool socd_ud = ((input & BATTLE_INPUT_UP) != 0) &&  ((input & BATTLE_INPUT_DOWN) != 0);
	if (socd_ud) {
		// clean bits
		input ^= BATTLE_INPUT_UP;
		input ^= BATTLE_INPUT_DOWN;
	}
	return input;
}

struct BattleInputs battle_read_input(struct Inputs const *inputs)
{
	struct BattleInputs input = {0};

	if (inputs->buttons_is_down[InputButtons_W]) {
		input.player1 |= BATTLE_INPUT_UP;
	}
	if (inputs->buttons_is_down[InputButtons_A]) {
		input.player1 |= BATTLE_INPUT_BACK;
	}
	if (inputs->buttons_is_down[InputButtons_S]) {
		input.player1 |= BATTLE_INPUT_DOWN;
	}
	if (inputs->buttons_is_down[InputButtons_D]) {
		input.player1 |= BATTLE_INPUT_FORWARD;
	}
	if (inputs->buttons_is_down[InputButtons_U]) {
		input.player1 |= BATTLE_INPUT_LPUNCH;
	}
	if (inputs->buttons_is_down[InputButtons_I]) {
		input.player1 |= BATTLE_INPUT_RPUNCH;
	}
	if (inputs->buttons_is_down[InputButtons_J]) {
		input.player1 |= BATTLE_INPUT_LKICK;
	}
	if (inputs->buttons_is_down[InputButtons_K]) {
		input.player1 |= BATTLE_INPUT_RKICK;
	}

	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_DPAD_UP]) {
		input.player2 |= BATTLE_INPUT_UP;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_DPAD_RIGHT]) {
		input.player2 |= BATTLE_INPUT_BACK;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_DPAD_DOWN]) {
		input.player2 |= BATTLE_INPUT_DOWN;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_DPAD_LEFT]) {
		input.player2 |= BATTLE_INPUT_FORWARD;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_WEST]) {
		input.player2 |= BATTLE_INPUT_LPUNCH;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_NORTH]) {
		input.player2 |= BATTLE_INPUT_RPUNCH;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_SOUTH]) {
		input.player2 |= BATTLE_INPUT_LKICK;
	}
	if (inputs->gamepad_buttons_is_down[InputGamepadButtons_EAST]) {
		input.player2 |= BATTLE_INPUT_RKICK;
	}

	input.player1 = battle_clean_socd_input(input.player1);
	input.player2 = battle_clean_socd_input(input.player2);

	return input;
}

// -- Battle main functions

enum BattleFrameResult battle_state_update(struct BattleContext *ctx, struct BattleInputs inputs);

enum BattleFrameResult battle_simulate_frame(struct BattleContext *ctx, struct BattleInputs inputs)
{
	TracyCZoneN(f, "Battle - simulate frame", true);
	enum BattleFrameResult result = battle_state_update(ctx, inputs);

	// desync checks

	// Notify ggpo that we've moved forward exactly 1 frame.
	// ggpo_advance_frame

	// ggpoutil_perfmon_update
	TracyCZoneEnd(f);

	return result;
}

void battle_state_init_player(struct BattleContext *ctx, struct PlayerEntity *p, struct PlayerNonEntity *pn)
{
	struct tek_Character *c = tek_characters + p->tek.character_id;
	SkeletalMeshAsset const *skeletal_mesh = asset_library_get_skeletal_mesh(ctx->assets, c->skeletal_mesh_id);
	AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(ctx->assets, c->anim_skeleton_id);

	// create an instance to hold the render pose
	skeletal_mesh_create_instance(skeletal_mesh, &pn->mesh_instance, anim_skeleton);
	// create render instances
	struct SkeletalMeshInstanceData render_instance_data = {0};
	render_instance_data.mesh = skeletal_mesh;
	render_instance_data.dynamic_data_mesh = &pn->mesh_instance;
	render_instance_data.dynamic_data_spatial = &p->spatial;
	renderer_register_skeletal_mesh_instance(ctx->renderer, render_instance_data);
}

void battle_state_init(struct BattleContext *ctx)
{
	battle_state_new_round(ctx);

	struct BattleState *state = &ctx->battle_state;
	struct BattleNonState *nonstate = &ctx->battle_non_state;
	battle_state_init_player(ctx, &state->p1_entity, &nonstate->p1_nonentity);
	battle_state_init_player(ctx, &state->p2_entity, &nonstate->p2_nonentity);
}

void battle_state_new_round(struct BattleContext *ctx)
{
	struct BattleState *state = &ctx->battle_state;

	// Reset spatial components
	Float3 p1_initial_pos = (Float3){-1.1f, 0.f, 0.f};
	Float3 p2_initial_pos = (Float3){ 1.1f, 0.f, 0.f};
	spatial_component_set_position(&state->p1_entity.spatial, p1_initial_pos);
	state->p1_entity.spatial.world_transform.cols[0] = (Float3){1.0f, 0.0f, 0.0f};
	state->p1_entity.spatial.world_transform.cols[1] = (Float3){0.0f, 1.0f, 0.0f};
	state->p1_entity.spatial.world_transform.cols[2] = (Float3){0.0f, 0.0f, 1.0f};
	spatial_component_target(&state->p1_entity.spatial, p2_initial_pos);
	spatial_component_set_position(&state->p2_entity.spatial, p2_initial_pos);
	state->p2_entity.spatial.world_transform.cols[0] = (Float3){1.0f, 0.0f, 0.0f};
	state->p2_entity.spatial.world_transform.cols[1] = (Float3){0.0f, 1.0f, 0.0f};
	state->p2_entity.spatial.world_transform.cols[2] = (Float3){0.0f, 0.0f, 1.0f};
	spatial_component_target(&state->p2_entity.spatial, p1_initial_pos);

	// Reset tek component
	struct tek_Character *c1 = tek_characters + state->p1_entity.tek.character_id;
	struct tek_Character *c2 = tek_characters + state->p2_entity.tek.character_id;
	state->p1_entity.tek.hp = c1->max_health;
	state->p1_entity.tek.current_move_id = c1->moves[0].id;
	state->p2_entity.tek.hp = c2->max_health;
	state->p2_entity.tek.current_move_id = c2->moves[0].id;

	// Reset animation
	int const idle_anim_id = 1775356884;
	state->p1_entity.anim_skeleton.anim_skeleton_id = c1->anim_skeleton_id;
	state->p1_entity.animation.animation_id = idle_anim_id; // idle
	state->p1_entity.animation.frame = 0;

	state->p2_entity.anim_skeleton.anim_skeleton_id = c2->anim_skeleton_id;
	state->p2_entity.animation.animation_id = idle_anim_id; // idle
	state->p2_entity.animation.frame = 0;

	// Reset camera
	struct BattleNonState *nonstate = &ctx->battle_non_state;
	nonstate->camera_distance = 3.0f;
	nonstate->camera.position = (Float3){1.0f, -5.0f, 1.0f};
	nonstate->camera.vertical_fov = 40.0f;
	nonstate->camera_focus = 1;
}

void battle_state_term(struct BattleContext *ctx)
{
	renderer_clear_skeletal_mesh_instances(ctx->renderer);
}


static bool match_cancel(struct TekPlayerComponent player, struct tek_Cancel cancel, struct CancelContext ctx)
{
	if (cancel.to_move_id == 0) {
		// end of list?
		return false;
	}

	/** Frame window checks:
	    - if the cancel does not specify inputs:
		- if cancel is EOA -> set starting frame to end
	        - cancel if animation_frame > starting_frame
	    - if the cancel specify inputs:
	        - animation frame has to be in input window range
		- no window -> input window range is [0, animation_length]
	**/
	uint32_t starting_frame = cancel.starting_frame;
	if (cancel.condition == TEK_CANCEL_CONDITION_END_OF_ANIMATION) {
		starting_frame = ctx.animation_length;
	}
	bool is_in_frame = ctx.animation_frame >= starting_frame;

	uint32_t input_window_start = cancel.input_window_start;
	uint32_t input_window_end = cancel.input_window_end;
	bool is_in_input_window = input_window_start <= ctx.animation_frame && ctx.animation_frame <= input_window_end;
	if (cancel.input_window_start == 0 && cancel.input_window_end == 0) {
		is_in_input_window = true;
	}

	/** Input checks: **/
	bool match_dir = true;
	bool match_action = true;
	if (cancel.motion_input != 0 || cancel.action_input != 0) {
		// current frame input
		uint32_t current_start_frame = player.input_buffer_frame_start[player.current_input_index % INPUT_BUFFER_SIZE];
		uint32_t current_input_index = (player.current_input_index + (INPUT_BUFFER_SIZE - 0)) % INPUT_BUFFER_SIZE;
		enum BattleInputBits current_input = player.input_buffer[current_input_index];
		// previous input (not last frame!)
		uint32_t previous_input_index = (player.current_input_index + INPUT_BUFFER_SIZE - 1) % INPUT_BUFFER_SIZE;
		enum BattleInputBits previous_input = player.input_buffer[previous_input_index];
		// previous previous input (not frame n-2!)
		uint32_t previous_previous_input_index = (player.current_input_index + INPUT_BUFFER_SIZE - 2) % INPUT_BUFFER_SIZE;
		enum BattleInputBits previous_previous_input = player.input_buffer[previous_previous_input_index];
		uint32_t previous_previous_start_frame = player.input_buffer_frame_start[previous_previous_input_index];

		BattleInput ACTION_MASK = (BATTLE_INPUT_LPUNCH | BATTLE_INPUT_RPUNCH | BATTLE_INPUT_LKICK | BATTLE_INPUT_RKICK);
		BattleInput action_input = current_input & ACTION_MASK;
		tek_MotionInput current_motion = 0;
		if ((current_input & BATTLE_INPUT_BACK) != 0) {
			current_motion = TEK_MOTION_INPUT_B;

			uint32_t frame_window = ctx.current_frame -  (previous_previous_start_frame + 1);
			if (previous_input == 0 && previous_previous_input == BATTLE_INPUT_BACK && frame_window <= 15) {
				current_motion = TEK_MOTION_INPUT_BB;
			}
		} else if ((current_input & BATTLE_INPUT_UP) != 0) {
			current_motion = TEK_MOTION_INPUT_U;
		} else if ((current_input & BATTLE_INPUT_FORWARD) != 0) {
			current_motion = TEK_MOTION_INPUT_F;

			uint32_t frame_window = ctx.current_frame -  (previous_previous_start_frame + 1);
			tek_MotionInput possible_motion = current_motion;
			if (previous_input == 0 && previous_previous_input == BATTLE_INPUT_FORWARD && frame_window <= 10) {
				possible_motion = TEK_MOTION_INPUT_FF;
			} else if ((current_input & BATTLE_INPUT_DOWN) != 0) {
				possible_motion = TEK_MOTION_INPUT_DF;
			}
			if (cancel.motion_input == possible_motion) {
				current_motion = possible_motion;
			}
		} else if ((current_input & BATTLE_INPUT_DOWN) != 0) {
			current_motion = TEK_MOTION_INPUT_D;
		}

		match_dir = current_motion  == cancel.motion_input;
		match_action = action_input == cancel.action_input;
	}

	return match_dir & match_action & is_in_frame & is_in_input_window;
}

static void player_translate_world(uint32_t iplayer, struct PlayerEntity **players, struct tek_Character **characters, uint32_t players_length, Float3 world_translate)
{
	float player_radius = characters[iplayer]->colision_radius;
	Float3 player_position = players[iplayer]->spatial.world_transform.cols[3];

	Float3 target_position = float3_add(player_position, world_translate);

	bool found_col = false;
	Float3 closest_position = {};
	float closest_distance = -100.0f;
	for (uint32_t iother = 0; iother < players_length; ++iother) {
		if (iother != iplayer) {
			float other_radius = characters[iother]->colision_radius;
			Float3 other_position = players[iother]->spatial.world_transform.cols[3];

			float distance = float3_distance(target_position, other_position);
			float dd = distance - (player_radius + other_radius);
			if (dd < 0 && dd > closest_distance) {
				found_col = true;
				closest_position = other_position;
				closest_distance = dd;
			}
		}
	}

	float coef = 1.0f;
	if (found_col && closest_distance < 0.0f) {
		coef = 0.0f;
	}

	players[iplayer]->spatial.world_transform.cols[3] = float3_add(player_position, float3_mul_scalar(world_translate, coef));
}

static void _evaluate_hit_conditions(struct PlayerEntity *p1, struct PlayerEntity *p2, struct PlayerNonEntity *np1, struct PlayerNonEntity *np2, struct tek_Character *c1, struct tek_Character *c2)
{
	struct tek_Move *current_move = tek_character_find_move(c1, p1->tek.current_move_id);

	if (current_move && current_move->hit_conditions_length > 0) {
		uint32_t current = p1->animation.frame;
		uint32_t first_active = current_move->startup;
		uint32_t last_active = current_move->startup + current_move->active;
		bool is_active = first_active <= current && current < last_active;
		if (is_active) {
			assert(current_move->hitbox < c1->hitboxes_length);
			uint32_t ihitbox = current_move->hitbox;
			Float3 hit_center = np1->hitboxes_position[ihitbox];
			float hit_radius = c1->hitboxes_radius[ihitbox];
			float hit_height = c1->hitboxes_height[ihitbox];

			if (hit_radius > 0.0f) {
				for (uint32_t ihurtbox = 0; ihurtbox < c2->hurtboxes_length; ++ihurtbox){
					Float3 hurt_center = np2->hurtboxes_position[ihurtbox];
					float hurt_radius = c2->hurtboxes_radius[ihurtbox];
					float hurt_height = c2->hurtboxes_height[ihurtbox];

					float x_dist = hit_center.x - hurt_center.x;
					float y_dist = hit_center.y - hurt_center.y;
					float hor_distance = sqrtf(x_dist*x_dist + y_dist*y_dist);
					float vert_distance = fabs(hit_center.z - hurt_center.z);

					bool is_valid = hurt_radius > 0.0f;
					bool inside_vert = vert_distance <= ((hit_height + hurt_height) / 2.0f);
					bool inside_hor = hor_distance <= (hit_radius + hurt_radius);
					bool inside = is_valid & inside_vert & inside_hor;
					if (inside) {
						printf("HIIIIIIIIIIIIIIIIIIT hurtbox[%u] distance: %fx%f\n", ihurtbox, hor_distance, vert_distance);

						struct tek_HitCondition hit_condition = current_move->hit_conditions[0];
						struct tek_HitReactions *hit_reaction = tek_character_find_hit_reaction(c1, hit_condition.reactions_id);


						bool is_blocking = p2->tek.current_move_id == TEK_MOVE_BACKWARD_LOOP_ID;
						if (is_blocking) {
							p2->tek.requested_move_id = hit_reaction->standing_block_move;

							p2->tek.pushback_remaining_frames = 3;
							p2->tek.pushback_strength = 0.1f;
						} else {
							p2->tek.hp -= hit_condition.damage;
							p2->tek.requested_move_id = hit_reaction->standing_move;
						}

						if (p2->tek.hp <= 0) {
							p2->tek.requested_move_id = TEK_MOVE_DEATH_ID;
						}
					}
				}
			}

		}
	}
}

enum BattleFrameResult battle_state_update(struct BattleContext *ctx, struct BattleInputs inputs)
{
	TracyCZoneN(f, "StateUpdate", true);

	struct BattleState *state = &ctx->battle_state;
	struct BattleNonState *nonstate = &ctx->battle_non_state;
	struct PlayerEntity *players[] = {&state->p1_entity, &state->p2_entity};
	struct PlayerNonEntity *nonplayers[] = {
		&nonstate->p1_nonentity,
		&nonstate->p2_nonentity,
	};
	struct tek_Character *characters[] = {
		tek_characters + players[0]->tek.character_id,
		tek_characters + players[1]->tek.character_id,
	};
	BattleInput pinputs[] = {inputs.player1, inputs.player2};

	// register input in the input buffer
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		struct PlayerEntity *p = players[iplayer];
		if (pinputs[iplayer] != p->tek.input_buffer[p->tek.current_input_index % INPUT_BUFFER_SIZE]) {
			p->tek.current_input_index += 1;
			uint32_t input_index = p->tek.current_input_index % INPUT_BUFFER_SIZE;
			p->tek.input_buffer[input_index] = pinputs[iplayer];
			p->tek.input_buffer_frame_start[input_index] = state->frame_number;
		}
	}

	// -- gameplay update
	// apply pushback
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		if (players[iplayer]->tek.pushback_remaining_frames > 0) {
			Float3 translation = {0};
			translation.x = players[iplayer]->tek.pushback_strength;
			if (iplayer == 0) {
				translation.x = -translation.x;
			}

			player_translate_world(iplayer, players, characters, ARRAY_LENGTH(players), translation);
			players[iplayer]->tek.pushback_remaining_frames -= 1;
		}
	}


	// apply requested move from previous simulation
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		uint32_t requested_move_id = players[iplayer]->tek.requested_move_id;
		if (requested_move_id != 0) {
			struct tek_Move *request_move = tek_character_find_move(characters[iplayer], requested_move_id);
			struct Animation const *animation = asset_library_get_animation(ctx->assets, request_move->animation_id);

			players[iplayer]->animation.animation_id = request_move->animation_id;
			players[iplayer]->animation.frame = 0;
			players[iplayer]->tek.current_move_id = requested_move_id;
			players[iplayer]->tek.requested_move_id = 0;
		}
	}

	// move request
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		// find current move
		struct tek_Move *current_move = tek_character_find_move(characters[iplayer], players[iplayer]->tek.current_move_id);
		struct tek_Cancel *current_cancels = current_move->cancels;

		// find valid cancel
		struct Animation const *animation = asset_library_get_animation(ctx->assets, current_move->animation_id);
		struct CancelContext cancel_ctx = {0};
		cancel_ctx.current_frame = state->frame_number;
		cancel_ctx.animation_frame = players[iplayer]->animation.frame;
		cancel_ctx.animation_length = animation->root_motion_track.translations.length;
		uint32_t request_move_id = 0;

		struct tek_Cancel *cancel = NULL;
		for (uint32_t imovecancel = 0; imovecancel < current_move->cancels_length; ++imovecancel) {
			bool is_group = current_cancels[imovecancel].type == TEK_CANCEL_TYPE_LIST;

			if (!is_group) {
				if (match_cancel(players[iplayer]->tek, current_cancels[imovecancel], cancel_ctx)) {
					cancel = &current_cancels[imovecancel];
				}
			} else {

				uint32_t starting_frame = current_cancels[imovecancel].starting_frame;
				if (current_cancels[imovecancel].condition == TEK_CANCEL_CONDITION_END_OF_ANIMATION) {
					starting_frame = cancel_ctx.animation_length;
				}
				bool is_in_frame = cancel_ctx.animation_frame >= starting_frame;

				uint32_t input_window_start = current_cancels[imovecancel].input_window_start;
				uint32_t input_window_end = current_cancels[imovecancel].input_window_end;
				bool is_in_input_window = input_window_start <= cancel_ctx.animation_frame && cancel_ctx.animation_frame <= input_window_end;
				if (current_cancels[imovecancel].input_window_start == 0 && current_cancels[imovecancel].input_window_end == 0) {
					is_in_input_window = true;
				}

				struct tek_CancelGroup *group = NULL;
				// find group
				for (uint32_t imovecancelgroup = 0; imovecancelgroup < characters[iplayer]->cancel_groups_length; ++imovecancelgroup) {
					if (characters[iplayer]->cancel_groups[imovecancelgroup].id == current_cancels[imovecancel].to_move_id) {
						group = &characters[iplayer]->cancel_groups[imovecancelgroup];
						break;
					}
				}
				// if found, match list
				if (group && is_in_frame) {
					for (uint32_t igroupcancel = 0; igroupcancel < MAX_CANCELS_PER_GROUP; ++igroupcancel) {
						struct tek_Cancel *groupcancel = &group->cancels[igroupcancel];
						if (match_cancel(players[iplayer]->tek, *groupcancel, cancel_ctx)) {
							cancel = groupcancel;
							break;
						}
					}
				}
			}
			if (cancel) {
				struct tek_Move *request_move = tek_character_find_move(characters[iplayer], cancel->to_move_id);
				if (current_move->id != cancel->to_move_id) {
					printf("p%u cancel %s[%u] -> %s[%u] | frame: %u | anim frame: %u | anim len: %u\n",
					       iplayer,
					       characters[iplayer]->move_names[current_move-characters[iplayer]->moves].string,
					       current_move->id,
					       characters[iplayer]->move_names[request_move-characters[iplayer]->moves].string,
					       cancel->to_move_id,
					       state->frame_number,
					       cancel_ctx.animation_frame,
					       cancel_ctx.animation_length
					       );
				}
				request_move_id = cancel->to_move_id;
				break;
			}
		}
		if (request_move_id != 0) {
			// find request move
			struct tek_Move *request_move = tek_character_find_move(characters[iplayer], request_move_id);
			// if found, perform the cancel
			if (request_move != NULL) {
				struct Animation const *animation = asset_library_get_animation(ctx->assets, request_move->animation_id);
				players[iplayer]->animation.animation_id = request_move->animation_id;
				if (cancel->type == TEK_CANCEL_TYPE_SINGLE_LOOP || cancel->type == TEK_CANCEL_TYPE_SINGLE_CONTINUE) {
					players[iplayer]->animation.frame = players[iplayer]->animation.frame % cancel_ctx.animation_length;
				} else {
					players[iplayer]->animation.frame = 0;
				}
				players[iplayer]->tek.current_move_id = request_move->id;
			}
		}
	}

	// Camera follow
	if (nonstate->camera_focus != 0)
	{
		struct SpatialComponent *p1_root = &state->p1_entity.spatial;
		struct SpatialComponent *p2_root = &state->p2_entity.spatial;

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

		float const CAMERA_HACK_TWEAK = 0.35f;
		float ideal_width = float3_distance(p1_root->world_transform.cols[3], p2_root->world_transform.cols[3]) * CAMERA_HACK_TWEAK;

		float ideal_distance = ideal_width / width_from_dist;

		Float3 camera_dir = p1_root->world_transform.cols[0];
		if (nonstate->camera_focus == 2) {
			camera_dir = p2_root->world_transform.cols[0];
		}

		float const MINIMUM_DISTANCE = 3.5f;
		float const CAMERA_SMOOTHING = 0.1f;

		float delta_dist = (ideal_distance - nonstate->camera_distance) * CAMERA_SMOOTHING;
		nonstate->camera_distance += delta_dist;
		if (nonstate->camera_distance < MINIMUM_DISTANCE) {
			nonstate->camera_distance = MINIMUM_DISTANCE;
		}

		nonstate->camera.position = float3_add(target_pos, float3_mul_scalar(camera_dir, nonstate->camera_distance));
		nonstate->camera.lookat = target_pos;

		nonstate->camera.position.z += 1.0f;
		nonstate->camera.lookat.z += 1.0f;
	}

	// Evaluate anims
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		struct AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(ctx->assets, players[iplayer]->anim_skeleton.anim_skeleton_id);
		struct Animation const *animation = asset_library_get_animation(ctx->assets, players[iplayer]->animation.animation_id);
		// Evaluate animation and apply root motion
		if (animation != NULL) {
			anim_evaluate_animation(anim_skeleton, animation, &nonplayers[iplayer]->pose, players[iplayer]->animation.frame);
			// update render skeleton
			anim_pose_compute_global_transforms(anim_skeleton, &nonplayers[iplayer]->pose);
			skeletal_mesh_apply_pose(&nonplayers[iplayer]->mesh_instance, &nonplayers[iplayer]->pose);
			// use root motion
			Float3 root_translation = float3x4_transform_direction(players[iplayer]->spatial.world_transform, nonplayers[iplayer]->pose.root_motion_delta_translation);
			player_translate_world(iplayer, players, characters, ARRAY_LENGTH(players), root_translation);
		}
		// Update animation component
		players[iplayer]->animation.frame += 1;
		// update hurtboxes positions
		for (uint32_t ihurtbox = 0; ihurtbox < characters[iplayer]->hurtboxes_length; ++ihurtbox) {
			uint32_t hurtbox_bone_id = characters[iplayer]->hurtboxes_bone_id[ihurtbox];
			for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ++ibone){
				if (anim_skeleton->bones_identifier[ibone] == hurtbox_bone_id) {
					nonplayers[iplayer]->hurtboxes_position[ihurtbox] = float3x4_transform_point(
														     players[iplayer]->spatial.world_transform,
														     nonplayers[iplayer]->pose.global_transforms[ibone].cols[3]);
					break;
				}
			}
		}
		// update hitboxes positions
		for (uint32_t ihitbox = 0; ihitbox < characters[iplayer]->hitboxes_length; ++ihitbox) {
			uint32_t hitbox_bone_id = characters[iplayer]->hitboxes_bone_id[ihitbox];
			for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ++ibone){
				if (anim_skeleton->bones_identifier[ibone] == hitbox_bone_id) {
					nonplayers[iplayer]->hitboxes_position[ihitbox] = float3x4_transform_point(
														     players[iplayer]->spatial.world_transform,
														     nonplayers[iplayer]->pose.global_transforms[ibone].cols[3]);
					break;
				}
			}
		}
	}

	_evaluate_hit_conditions(players[0], players[1], nonplayers[0], nonplayers[1], characters[0], characters[1]);
	_evaluate_hit_conditions(players[1], players[0], nonplayers[1], nonplayers[0], characters[1], characters[0]);


	// post update: determine future game state
	enum BattleFrameResult frame_result = BATTLE_FRAME_RESULT_CONTINUE;

	// someone won a round?
	if (players[0]->tek.hp <= 0) {
		// TODO: outro, now we wait 60 frames to finish
		if (players[0]->animation.frame > 60) {
			nonstate->rounds_p2_won += 1;
			battle_state_new_round(ctx);
		}
	}
	// someone won the match?
	if (nonstate->rounds_p2_won >= nonstate->rounds_first_to) {
		frame_result = BATTLE_FRAME_RESULT_END;
	}
	if (players[1]->tek.hp <= 0) {
		// TODO: outro, now we wait 60 frames to finish
		if (players[1]->animation.frame > 60) {
			nonstate->rounds_p1_won += 1;
			battle_state_new_round(ctx);
		}
	}
	// someone won the match?
	if (nonstate->rounds_p1_won >= nonstate->rounds_first_to) {
		frame_result = BATTLE_FRAME_RESULT_END;
	}

	// next frame
	state->frame_number += 1;

	TracyCZoneEnd(f);

	return frame_result;
}

// -- Render

void battle_render(struct BattleContext *ctx)
{
	TracyCZoneN(f, "BattleRender", true);
	struct BattleState *state = &ctx->battle_state;
	struct BattleNonState *nonstate = &ctx->battle_non_state;
	struct PlayerEntity *players[] = {&state->p1_entity, &state->p2_entity};
	struct PlayerNonEntity *nonplayers[] = {
		&nonstate->p1_nonentity,
		&nonstate->p2_nonentity,
	};
	struct tek_Character *characters[] = {
		tek_characters + players[0]->tek.character_id,
		tek_characters + players[1]->tek.character_id,
	};

	ed_display_player_entity("Player 1", &state->p1_entity);
	ed_display_player_entity("Player 2", &state->p2_entity);

	if (ImGui_Begin("Debug", NULL, 0)) {
		ImGui_DragFloat3Ex("camera position", &nonstate->camera.position.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat3Ex("camera lookat", &nonstate->camera.lookat.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat("camera fov", &nonstate->camera.vertical_fov);
		ImGui_DragInt("camera focus", &nonstate->camera_focus);
		ImGui_Checkbox("draw grid", &nonstate->draw_grid);
		ImGui_Checkbox("draw hurtboxes", &nonstate->draw_hurtboxes);
		ImGui_Checkbox("draw hitboxes", &nonstate->draw_hitboxes);
		ImGui_Checkbox("draw colisions", &nonstate->draw_colisions);
	}
	ImGui_End();

	// -- debug draw
	debug_draw_reset();

	// Evaluate anims
	if (nonstate->draw_bones) {
		for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
			struct AnimSkeleton const *anim_skeleton = asset_library_get_anim_skeleton(ctx->assets, players[iplayer]->anim_skeleton.anim_skeleton_id);
			struct Animation const *animation = asset_library_get_animation(ctx->assets, players[iplayer]->animation.animation_id);
			// Debug draw animated pose
			for (uint32_t ibone = 0; ibone < anim_skeleton->bones_length; ibone++) {
				Float3 p;
				p.x = F34(nonplayers[iplayer]->pose.global_transforms[ibone], 0, 3);
				p.y = F34(nonplayers[iplayer]->pose.global_transforms[ibone], 1, 3);
				p.z = F34(nonplayers[iplayer]->pose.global_transforms[ibone], 2, 3);
				p = float3x4_transform_point(players[iplayer]->spatial.world_transform, p);
				debug_draw_point(p);
			}
		}
	}

	// debug draw hurtboxes cylinders
	if (nonstate->draw_hurtboxes) {
		for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
			for (uint32_t ihurtbox = 0; ihurtbox < characters[iplayer]->hurtboxes_length; ++ihurtbox) {
				Float3 center = nonplayers[iplayer]->hurtboxes_position[ihurtbox];
				float radius = characters[iplayer]->hurtboxes_radius[ihurtbox];
				float height = characters[iplayer]->hurtboxes_height[ihurtbox];
				debug_draw_cylinder(center, radius, height, DD_RED);
			}
		}
	}

	// debug draw hitboxes cylinders
	if (nonstate->draw_hitboxes) {
		for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
			struct tek_Move *current_move = tek_character_find_move(characters[iplayer], players[iplayer]->tek.current_move_id);

			uint32_t current = players[iplayer]->animation.frame;
			uint32_t first_active = current_move->startup;
			uint32_t last_active = current_move->startup + current_move->active;
			bool is_active = first_active <= current && current < last_active;
			if (is_active) {
				assert(current_move->hitbox < characters[iplayer]->hitboxes_length);
				uint32_t ihitbox = current_move->hitbox;
				Float3 center = nonplayers[iplayer]->hitboxes_position[ihitbox];
				float radius = characters[iplayer]->hitboxes_radius[ihitbox];
				float height = characters[iplayer]->hitboxes_height[ihitbox];
				debug_draw_cylinder(center, radius, height, DD_GREEN);
			}
		}
	}

	if (nonstate->draw_colisions) {
		for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
			float radius = characters[iplayer]->colision_radius;
			Float3 center = players[iplayer]->spatial.world_transform.cols[3];
			float height = 2.0;
			center.z += height * 0.5;
			debug_draw_cylinder(center, radius, height, DD_GREEN);
		}
	}
	// debug draw grid
	if (nonstate->draw_grid) {
		float width = 24.0f;
		for (float i = 1.0f; i <= width; i += 1.0f) {
			debug_draw_line((Float3){i, -width, 0.0f}, (Float3){i, width, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
			debug_draw_line((Float3){-i, -width, 0.0f}, (Float3){-i, width, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
			debug_draw_line((Float3){-width, i, 0.0f}, (Float3){width, i, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
			debug_draw_line((Float3){-width, -i, 0.0f}, (Float3){width, -i, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
		}
		debug_draw_line((Float3){-width, 0.0f, 0.0f}, (Float3){0, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
		debug_draw_line((Float3){0, 0.0f, 0.0f}, (Float3){width, 0.0f, 0.0f}, DD_RED);
		debug_draw_line((Float3){0.0f, -width, 0.0f}, (Float3){0, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
		debug_draw_line((Float3){0, 0.0f, 0.0f}, (Float3){0.0f, width, 0.0f}, DD_GREEN);
		debug_draw_line((Float3){0.0f, 0.0f, -width}, (Float3){0.0f, 0.0f, 0.0f}, DD_WHITE & DD_HALF_ALPHA);
		debug_draw_line((Float3){0.0f, 0.0f, 0.0f}, (Float3){0.0f, 0.0f, width}, DD_BLUE);
		width = 2.4f;
		for (float i = 0.1f; i <= width; i += 0.1f) {
			debug_draw_line((Float3){i, -width, 0.0f}, (Float3){i, width, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
			debug_draw_line((Float3){-i, -width, 0.0f}, (Float3){-i, width, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
			debug_draw_line((Float3){-width, i, 0.0f}, (Float3){width, i, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
			debug_draw_line((Float3){-width, -i, 0.0f}, (Float3){width, -i, 0.0f}, DD_WHITE & DD_QUARTER_ALPHA);
		}
	}
	// draw local axis for players
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		struct SpatialComponent *root = &players[iplayer]->spatial;
		Float3 origin = {0};
		Float3 x_axis = (Float3){0.1f, 0.0f, 0.0f};
		Float3 y_axis = (Float3){0.0f, 0.1f, 0.0f};
		Float3 z_axis = (Float3){0.0f, 0.0f, 0.1f};
		Float3 o = float3x4_transform_point(root->world_transform, origin);
		Float3 x = float3x4_transform_point(root->world_transform, x_axis);
		Float3 y = float3x4_transform_point(root->world_transform, y_axis);
		Float3 z = float3x4_transform_point(root->world_transform, z_axis);
		debug_draw_line(o, x, DD_RED);
		debug_draw_line(o, y, DD_GREEN);
		debug_draw_line(o, z, DD_BLUE);
	}


	// interpolate camera for smooth movement
	float coef = 0.05f;
	nonstate->camera_display.position.x = (nonstate->camera_display.position.x)*(1.0-coef) + (nonstate->camera.position.x)*(coef);
	nonstate->camera_display.position.y = (nonstate->camera_display.position.y)*(1.0-coef) + (nonstate->camera.position.y)*(coef);
	nonstate->camera_display.position.z = (nonstate->camera_display.position.z)*(1.0-coef) + (nonstate->camera.position.z)*(coef);
	nonstate->camera_display.lookat.x = (nonstate->camera_display.lookat.x)*(1.0-coef) + (nonstate->camera.lookat.x)*(coef);
	nonstate->camera_display.lookat.y = (nonstate->camera_display.lookat.y)*(1.0-coef) + (nonstate->camera.lookat.y)*(coef);
	nonstate->camera_display.lookat.z = (nonstate->camera_display.lookat.z)*(1.0-coef) + (nonstate->camera.lookat.z)*(coef);
	nonstate->camera_display.vertical_fov = (nonstate->camera_display.vertical_fov)*(1.0-coef) + (nonstate->camera.vertical_fov)*(coef);
	renderer_set_main_camera(ctx->renderer, nonstate->camera_display);

	TracyCZoneEnd(f);
}
