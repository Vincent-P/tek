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
	AssetId animation_id;
};

struct SkeletonComponent
{
	AssetId anim_skeleton_id;
};
