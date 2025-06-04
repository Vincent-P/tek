#include "game_battle.h"
#include "inputs.h"
#include "debugdraw.h"
#include "renderer.h"
#include "editor.h"

struct CancelContext
{
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

struct BattleInputs battle_read_input(struct Inputs const *inputs)
{
	struct BattleInputs input = {0};

	if (inputs->buttons_is_pressed[InputButtons_W]) {
		input.player1 |= BATTLE_INPUT_UP;
	}
	if (inputs->buttons_is_pressed[InputButtons_A]) {
		input.player1 |= BATTLE_INPUT_BACK;
	}
	if (inputs->buttons_is_pressed[InputButtons_S]) {
		input.player1 |= BATTLE_INPUT_DOWN;
	}
	if (inputs->buttons_is_pressed[InputButtons_D]) {
		input.player1 |= BATTLE_INPUT_FORWARD;
	}
	if (inputs->buttons_is_pressed[InputButtons_U]) {
		input.player1 |= BATTLE_INPUT_LPUNCH;
	}
	if (inputs->buttons_is_pressed[InputButtons_I]) {
		input.player1 |= BATTLE_INPUT_RPUNCH;
	}
	if (inputs->buttons_is_pressed[InputButtons_J]) {
		input.player1 |= BATTLE_INPUT_LKICK;
	}
	if (inputs->buttons_is_pressed[InputButtons_K]) {
		input.player1 |= BATTLE_INPUT_RKICK;
	}
	return input;
}

// -- Battle main functions

enum BattleFrameResult battle_state_update(struct BattleContext *ctx, struct BattleInputs inputs);

enum BattleFrameResult battle_simulate_frame(struct BattleContext *ctx, struct BattleInputs inputs)
{
	TracyCZoneN(f, "Battle - simulate frame", true);
	debug_draw_reset();

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
	Float3 p1_initial_pos = (Float3){-2.0f, 0.f, 0.f};
	Float3 p2_initial_pos = (Float3){ 2.0f, 0.f, 0.f};
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
	state->p1_entity.anim_skeleton.anim_skeleton_id = c1->anim_skeleton_id;
	state->p1_entity.animation.animation_id = 3108588149; // idle
	state->p1_entity.animation.frame = 0;
	
	state->p2_entity.anim_skeleton.anim_skeleton_id = c2->anim_skeleton_id;
	state->p2_entity.animation.animation_id = 3108588149; // idle
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

	bool current_animation_end = ctx.animation_frame >= ctx.animation_length; // true at end only
	bool is_after_starting_frame = ctx.animation_frame >= cancel.starting_frame;  // true

	uint32_t input_index = (player.current_input_index + (INPUT_BUFFER_SIZE - 0)) % INPUT_BUFFER_SIZE;
	enum BattleInputBits frame_input = player.input_buffer[input_index];
	
	BattleInput ACTION_MASK = (BATTLE_INPUT_LPUNCH | BATTLE_INPUT_RPUNCH | BATTLE_INPUT_LKICK | BATTLE_INPUT_RKICK);	
	BattleInput action_input = frame_input & ACTION_MASK;

	tek_MotionInput current_motion = 0;
	if ((frame_input & BATTLE_INPUT_BACK) != 0) {
		current_motion = TEK_MOTION_INPUT_B;

		// look for another B to detect double tap back
		uint32_t previous_input_index = (player.current_input_index + INPUT_BUFFER_SIZE - 1) % INPUT_BUFFER_SIZE;
		uint32_t previous_previous_input_index = (player.current_input_index + INPUT_BUFFER_SIZE - 2) % INPUT_BUFFER_SIZE;
		enum BattleInputBits previous_input = player.input_buffer[previous_input_index];
		enum BattleInputBits previous_previous_input = player.input_buffer[previous_previous_input_index];
		uint32_t current_start_frame = player.input_buffer_frame_start[player.current_input_index % INPUT_BUFFER_SIZE];
		uint32_t previous_start_frame = player.input_buffer_frame_start[previous_input_index];
		uint32_t delta_frame = current_start_frame - (previous_start_frame + 1); // take only the last frame of the previous previous input as starting point.
		
		if (previous_input == 0 && previous_previous_input == BATTLE_INPUT_BACK && delta_frame <= 10) {
			current_motion = TEK_MOTION_INPUT_BB;
		}
		
	} else if ((frame_input & BATTLE_INPUT_UP) != 0) {
		current_motion = TEK_MOTION_INPUT_U;
	} else if ((frame_input & BATTLE_INPUT_FORWARD) != 0) {
		
		current_motion = TEK_MOTION_INPUT_F;

		// look for another F to detect double tap back
		uint32_t previous_input_index = (player.current_input_index + INPUT_BUFFER_SIZE - 1) % INPUT_BUFFER_SIZE;
		uint32_t previous_previous_input_index = (player.current_input_index + INPUT_BUFFER_SIZE - 2) % INPUT_BUFFER_SIZE;
		enum BattleInputBits previous_input = player.input_buffer[previous_input_index];
		enum BattleInputBits previous_previous_input = player.input_buffer[previous_previous_input_index];
		uint32_t current_start_frame = player.input_buffer_frame_start[player.current_input_index % INPUT_BUFFER_SIZE];
		uint32_t previous_start_frame = player.input_buffer_frame_start[previous_input_index];
		uint32_t delta_frame = current_start_frame - (previous_start_frame + 1); // take only the last frame of the previous previous input as starting point.
		
		if (previous_input == 0 && previous_previous_input == BATTLE_INPUT_FORWARD && delta_frame <= 10) {
			current_motion = TEK_MOTION_INPUT_FF;
		}
	} else if ((frame_input & BATTLE_INPUT_DOWN) != 0) {
		current_motion = TEK_MOTION_INPUT_D;
	}
	// TODO: match action even if dir don't match (jab while walking)
	
	bool match_dir = current_motion  == cancel.motion_input;
	bool match_action = action_input == cancel.action_input;
	bool end_of_animation = true;
	if (cancel.condition == TEK_CANCEL_CONDITION_END_OF_ANIMATION) {
		match_dir = true;
		match_action = true;
		end_of_animation = current_animation_end;
	}
	
	return match_dir & match_action & end_of_animation & is_after_starting_frame;
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
	// move request
	for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
		// find current move
		struct tek_Move *current_move = NULL;
		struct tek_Cancel *current_cancels = NULL;
		for (uint32_t imove = 0; imove < characters[iplayer]->moves_length; ++imove) {
			if (characters[iplayer]->moves[imove].id == players[iplayer]->tek.current_move_id) {
				current_move = &characters[iplayer]->moves[imove];
				current_cancels = current_move->cancels;
			}
		}
		// find valid cancel
		struct Animation const *animation = asset_library_get_animation(ctx->assets, current_move->animation_id);
		struct CancelContext cancel_ctx = {0};
		cancel_ctx.animation_frame = players[iplayer]->animation.frame;
		cancel_ctx.animation_length = animation->root_motion_track.translations.length;
		uint32_t request_move_id = 0;
		
		struct tek_Cancel *cancel = NULL;
		for (uint32_t imovecancel = 0; imovecancel < MAX_CANCELS_PER_MOVE; ++imovecancel) {
			bool is_group = current_cancels[imovecancel].type == TEK_CANCEL_TYPE_LIST;
			
			if (!is_group) {
				if (match_cancel(players[iplayer]->tek, current_cancels[imovecancel], cancel_ctx)) {
					cancel = &current_cancels[imovecancel];
				}
			} else {

				struct tek_CancelGroup *group = NULL;
				for (uint32_t imovecancelgroup = 0; imovecancelgroup < characters[iplayer]->cancel_groups_length; ++imovecancelgroup) {
					if (characters[iplayer]->cancel_groups[imovecancelgroup].id == current_cancels[imovecancel].to_move_id) {
						group = &characters[iplayer]->cancel_groups[imovecancelgroup];
						break;
					}
				}
				if (group) {
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
				struct tek_Move *request_move = NULL;
				for (uint32_t imove = 0; imove < characters[iplayer]->moves_length; ++imove) {
					if (characters[iplayer]->moves[imove].id == cancel->to_move_id) {
						request_move = &characters[iplayer]->moves[imove];
					}
				}
				if (iplayer == 0 && current_move->id != cancel->to_move_id) {
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
		// find request move
		struct tek_Move *request_move = NULL;
		for (uint32_t imove = 0; imove < characters[iplayer]->moves_length; ++imove) {
			if (characters[iplayer]->moves[imove].id == request_move_id) {
				request_move = &characters[iplayer]->moves[imove];
			}
		}
		// check if request move can be made
		if (current_move != NULL && request_move != NULL) {
			bool is_done = players[iplayer]->animation.frame > (current_move->startup + current_move->active + current_move->recovery);
			bool can_cancel = is_done;
			if (can_cancel) {
				// perform the cancel
				struct Animation const *animation = asset_library_get_animation(ctx->assets, request_move->animation_id);
				players[iplayer]->animation.animation_id = request_move->animation_id;
				if (cancel->type == TEK_CANCEL_TYPE_SINGLE_LOOP) {
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
			players[iplayer]->spatial.world_transform.cols[3] = float3_add(players[iplayer]->spatial.world_transform.cols[3], root_translation);
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

	// Evaluate hit
	{
		struct PlayerEntity *p1 = players[0];
		struct PlayerEntity *p2 = players[1];
		struct PlayerNonEntity *np1 = nonplayers[0];
		struct PlayerNonEntity *np2 = nonplayers[1];
		struct tek_Character *c1 = characters[0];
		struct tek_Character *c2 = characters[1];
		
		struct tek_Move *current_move = NULL;
		for (uint32_t imove = 0; imove < c1->moves_length; ++imove) {
			if (c1->moves[imove].id == p1->tek.current_move_id) {
				current_move = &c1->moves[imove];
			}
		}

		if (current_move) {
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

				for (uint32_t ihurtbox = 0; ihurtbox < c2->hurtboxes_length; ++ihurtbox){
					Float3 hurt_center = np2->hurtboxes_position[ihurtbox];
					float hurt_radius = c2->hurtboxes_radius[ihurtbox];
					float hurt_height = c2->hurtboxes_height[ihurtbox];

					float x_dist = hit_center.x - hurt_center.x;
					float y_dist = hit_center.y - hurt_center.y;
					float hor_distance = sqrtf(x_dist*x_dist + y_dist*y_dist);
					float vert_distance = fabs(hit_center.z - hurt_center.z);

					bool inside_vert = vert_distance <= ((hit_height + hurt_height) / 2.0f);
					bool inside_hor = hor_distance <= (hit_radius + hurt_radius);
					bool inside = inside_vert & inside_hor;
					if (inside) {
						printf("HIIIIIIIIIIIIIIIIIIT hurtbox[%u] distance: %fx%f\n", ihurtbox, hor_distance, vert_distance);
						p2->tek.hp -= current_move->base_damage;
					}
				}

			}
		}
	}

	// post update: determine future game state
	enum BattleFrameResult frame_result = BATTLE_FRAME_RESULT_CONTINUE;
	
	// someone won a round?
	if (players[1]->tek.hp <= 0) {
		// TODO: outro
		nonstate->rounds_p1_won += 1;
		battle_state_new_round(ctx);
	}
	// someone won the match?
	if (nonstate->rounds_p1_won >= nonstate->rounds_first_to) {
		frame_result = BATTLE_FRAME_RESULT_END;
	}

	
	// debug draw hurtboxes cylinders
	if (nonstate->draw_hurtboxes) {
		for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
			for (uint32_t ihurtbox = 0; ihurtbox < characters[iplayer]->hurtboxes_length; ++ihurtbox) {
				Float3 center = nonplayers[iplayer]->hurtboxes_position[ihurtbox];
				float radius = characters[iplayer]->hurtboxes_radius[ihurtbox];
				float height = characters[iplayer]->hurtboxes_height[ihurtbox];
				debug_draw_cylinder(center, radius, height, DD_GREEN);
			}
		}
	}
	if (nonstate->draw_hitboxes) {
		for (uint32_t iplayer = 0; iplayer < ARRAY_LENGTH(players); ++iplayer) {
			struct tek_Move *current_move = NULL;
			for (uint32_t imove = 0; imove < characters[iplayer]->moves_length; ++imove) {
				if (characters[iplayer]->moves[imove].id == players[iplayer]->tek.current_move_id) {
					current_move = &characters[iplayer]->moves[imove];
				}
			}

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
	
	struct TekPlayerComponent const *p1 = &state->p1_entity.tek;
	if (ImGui_Begin("Player 1 inputs", NULL, 0)) {
		if (ImGui_BeginTable("inputs", 2, ImGuiTableFlags_Borders)) {
			uint32_t input_last_frame = state->frame_number;
			for (uint32_t i = 0; i < INPUT_BUFFER_SIZE; i++) {
				uint32_t input_index = (p1->current_input_index + (INPUT_BUFFER_SIZE - i)) % INPUT_BUFFER_SIZE;
				enum BattleInputBits input = p1->input_buffer[input_index];
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

	if (ImGui_Begin("Battle", NULL, 0)) {
		ImGui_Text("First To %d", nonstate->rounds_first_to);
		ImGui_Text("P1: %d", nonstate->rounds_p1_won);
		ImGui_Text("P2: %d", nonstate->rounds_p2_won);
		ImGui_Text("");
		ImGui_DragFloat3Ex("camera position", &nonstate->camera.position.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat3Ex("camera lookat", &nonstate->camera.lookat.x, 0.1f, 0.0f, 0.0f, "%.3f", 0);
		ImGui_DragFloat("camera fov", &nonstate->camera.vertical_fov);
		ImGui_DragInt("camera focus", &nonstate->camera_focus);
		ImGui_Checkbox("draw grid", &nonstate->draw_grid);
		ImGui_Checkbox("draw hurtboxes", &nonstate->draw_hurtboxes);
		ImGui_Checkbox("draw hitboxes", &nonstate->draw_hitboxes);
	}
	ImGui_End();

	renderer_set_main_camera(ctx->renderer, nonstate->camera);

	TracyCZoneEnd(f);
}
