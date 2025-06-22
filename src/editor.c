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
	
	uint32_t i_current_move = 0;
	for (uint32_t imove = 0; imove < chara->moves_length; ++imove) {
		if (chara->moves[imove].id == player->current_move_id) {
			i_current_move = imove;
			break;
		}
	}
	
	ImGui_InputScalar("character", ImGuiDataType_U32, &player->character_id);
	ImGui_Text("current move: %s", chara->move_names[i_current_move].string);
	ImGui_InputScalar("current move last frame", ImGuiDataType_U32, &player->current_move_last_frame);
	ImGui_Text("Health"); ImGui_SameLine(); ImGui_ProgressBar((float)player->hp / (float)chara->max_health, (ImVec2){-FLT_MIN, 0}, NULL);

	ImGui_TextUnformatted("Moves:");
	for (uint32_t imove = 0; imove < chara->moves_length; ++imove) {
		struct tek_Move *move = &chara->moves[imove];
		ImGui_PushID((const char*)move);
		
		ImGui_Text("Move[%u]: %s", move->id, chara->move_names[imove].string);
		
		ImGui_SameLine();
		if (ImGui_Button("Do")) {
			player->current_move_id = move->id;
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
