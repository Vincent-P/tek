#include "editor.h"

void _display_spatial_component(struct SpatialComponent *spatial)
{
            ImGui_InputFloat3("position", &spatial->world_transform.cols[3].x);
}
void _display_skeleton_component(struct SkeletonComponent *skeleton)
{
}
void _display_animation_component(struct AnimationComponent *animation)
{
}
void _display_skeletal_mesh_component(struct SkeletalMeshComponent *mesh)
{
}
void _display_tek_component(struct TekPlayerComponent *player)
{
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
