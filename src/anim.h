#pragma once

#define MAX_BONES_PER_MESH 64
#define MAX_ANIMATIONS_PER_ASSET 64

typedef struct AnimTrack AnimTrack;
typedef struct Animation Animation;
typedef struct AnimSkeleton AnimSkeleton;

/**
animation skeleton:
  anim bones identifiers
  anim bones parent
  anim bones local

animation:
  bone_identifiers (per bone)
  tracks (per bone)
    positions[]
    rotations[]
    scales[]
  

skeletal mesh:
  vertices_position
  ...
  vertices_bone_indices
  vertices_bone_weights
  indices
  mesh bones geometry_to_bone (bind pose)
  mesh bones identifiers
  

RUNTIME
======
animation -> pose

Pose:
  skeleton ref
  local transforms
  global transforms (cached)
  state (unset, pose, reference, zero, additive)
  
runtime anim skeleton:
  skeleton ref
  anim bones world

the runtime anim skeleton is used to compute a pose, applied  to all character meshes
  
runtime skeletal mesh:
  mesh bones in world space
 **/
struct AnimSkeleton
{
	uint32_t id;
	uint32_t bones_length;
	uint32_t bones_identifier[MAX_BONES_PER_MESH];
	Float3x4 bones_local_transforms[MAX_BONES_PER_MESH];
	Float3x4 bones_global_transforms[MAX_BONES_PER_MESH];
	uint8_t bones_parent[MAX_BONES_PER_MESH];
};

struct AnimTrack
{
	struct Float3List translations;
	struct QuatList rotations;
	struct Float3List scales;
};

struct Animation
{
	struct AnimTrack root_motion_track;
	struct AnimTrack tracks[MAX_BONES_PER_MESH];
	uint32_t tracks_identifier[MAX_BONES_PER_MESH]; // debug only
	uint32_t tracks_length;	
	uint32_t skeleton_id;
};

enum AnimPoseState
{
	ANIM_POSE_STATE_UNSET = 0,
	ANIM_POSE_STATE_POSE,
	ANIM_POSE_STATE_ZERO,
	ANIM_POSE_STATE_REFERENCE,
	ANIM_POSE_STATE_ADDITIVE,
};

struct AnimPose
{
	uint32_t skeleton_id;
	Float3x4 local_transforms[MAX_BONES_PER_MESH];
	Float3x4 global_transforms[MAX_BONES_PER_MESH];
	enum AnimPoseState state;
};

void Serialize_AnimTrack(Serializer *serializer, AnimTrack *value);
void Serialize_Animation(Serializer *serializer, Animation *value);
void Serialize_AnimSkeleton(Serializer *serializer, AnimSkeleton *value);

void anim_evaluate_animation(struct AnimSkeleton *skeleton, Animation const* anim, struct AnimPose *out_pose, float t);
void anim_pose_compute_global_transforms(struct AnimSkeleton *skeleton, struct AnimPose *pose, Float3x4 root_world_transform);
