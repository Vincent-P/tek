#include "game_components.h"

void spatial_component_set_position(struct SpatialComponent *cpnt, Float3 pos)
{
	cpnt->world_transform.cols[3] = pos;
}

void spatial_component_target(struct SpatialComponent *cpnt, Float3 target_pos)
{
	Float3 pos = cpnt->world_transform.cols[3];
 	Float3 y = float3_normalize(float3_sub(target_pos, pos));
	Float3 x = float3_normalize(float3_cross(y, (Float3){0.0f, 0.0f, 1.0f}));
	Float3 z = float3_normalize(float3_cross(x, y));

	F34(cpnt->world_transform,0,0) = x.x;	F34(cpnt->world_transform,0,1) = y.x;	F34(cpnt->world_transform,0,2) = z.x;
	F34(cpnt->world_transform,1,0) = x.y;	F34(cpnt->world_transform,1,1) = y.y;	F34(cpnt->world_transform,1,2) = z.y;
	F34(cpnt->world_transform,2,0) = x.z;	F34(cpnt->world_transform,2,1) = y.z;	F34(cpnt->world_transform,2,2) = z.z;
}

