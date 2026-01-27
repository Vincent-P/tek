#include "editor.h"

void _display_spatial_component(struct SpatialComponent *spatial)
{
            ImGui_InputFloat3("x", &spatial->world_transform.cols[0].x);
            ImGui_InputFloat3("y", &spatial->world_transform.cols[1].x);
            ImGui_InputFloat3("z", &spatial->world_transform.cols[2].x);
            ImGui_InputFloat3("position", &spatial->world_transform.cols[3].x);
}
void _display_skeleton_component(struct SkeletonComponent *skeleton)
{
}
void _display_animation_component(struct AnimationComponent *animation)
{
	ImGui_InputScalar("frame", ImGuiDataType_U32, &animation->frame);
	ImGui_InputScalar("animation_id", ImGuiDataType_U32, &animation->animation_id);
}
void _display_skeletal_mesh_component(struct SkeletalMeshComponent *mesh)
{
}
void _display_tek_component(struct TekPlayerComponent *player)
{
	struct tek_Character *chara = tek_characters + player->character_id;

	uint32_t i_current_move = chara->moves_length;
	uint32_t i_requested_move = chara->moves_length;
	for (uint32_t imove = 0; imove < chara->moves_length; ++imove) {
		if (chara->moves[imove].id == player->current_move_id) {
			i_current_move = imove;
		}
		if (chara->moves[imove].id == player->requested_move_id) {
			i_requested_move = imove;
		}
	}

	ImGui_InputScalar("character", ImGuiDataType_U32, &player->character_id);
	ImGui_Text("current move: %s", chara->move_names[i_current_move].string);
	ImGui_Text("requested move: %s", chara->move_names[i_requested_move].string);
	ImGui_Text("Health"); ImGui_SameLine(); ImGui_ProgressBar((float)player->hp / (float)chara->max_health, (ImVec2){-FLT_MIN, 0}, NULL);

	ImGui_TextUnformatted("Moves:");
	for (uint32_t imove = 0; imove < chara->moves_length; ++imove) {
		struct tek_Move *move = &chara->moves[imove];
		ImGui_PushID((const char*)move);

		ImGui_Text("Move[%u]: %s | %s | %u cancels | %u hit conditions",
			   move->id,
			   chara->move_names[imove].string,
			   tek_HitLevel_str[move->hit_level],
			   move->cancels_length,
			   move->hit_conditions_length);

		ImGui_SameLine();
		if (ImGui_Button("Do")) {
			player->requested_move_id = move->id;
		}

		ImGui_PopID();
	}
}

void ed_display_player_entity(const char* id, struct PlayerEntity *player)
{
	if (ImGui_Begin(id, NULL, 0)) {
		if (ImGui_CollapsingHeader("spatial", 0)) {
			_display_spatial_component(&player->spatial);
		}
		if (ImGui_CollapsingHeader("anim_skeleton", 0)) {
			_display_skeleton_component(&player->anim_skeleton);
		}
		if (ImGui_CollapsingHeader("animation", 0)) {
			_display_animation_component(&player->animation);
		}
		if (ImGui_CollapsingHeader("mesh", 0)) {
			_display_skeletal_mesh_component(&player->mesh);
		}
		if (ImGui_CollapsingHeader("tek", 0)) {
			_display_tek_component(&player->tek);
		}
	}
	ImGui_End();
}

void ed_display_debug_menu(struct BattleNonState *nonstate)
{
	if (ImGui_Begin("Debug", NULL, 0)) {
		if (ImGui_Button("Reload characters")) {
			tek_read_character_json();
		}
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
}
