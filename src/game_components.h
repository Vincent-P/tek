#pragma once
#include "maths.h"

struct AABB
{
	Float3 min;
	Float3 max;
};

struct SpatialComponent
{
	struct AABB bounds;
	Float3x4 world_transform;
};

struct SkeletalMeshComponent
{
	// struct SpatialComponent spatial; how to reference other components? roots?
	AssetId skeletal_mesh_id;
};

struct AnimationComponent
{
	uint32_t frame;
	uint32_t previous_frame;
	AssetId animation_id;
	AssetId previous_animation_id;
	int32_t remaining_frames_to_blend; // blend with previous animation
};

struct SkeletonComponent
{
	AssetId anim_skeleton_id;
};


void spatial_component_set_position(struct SpatialComponent *cpnt, Float3 pos);
void spatial_component_target(struct SpatialComponent *cpnt, Float3 target_pos);
