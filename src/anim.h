#pragma once

#define MAX_BONES_PER_MESH 64
#define MAX_ANIMATIONS_PER_ASSET 64

typedef struct AnimTrack AnimTrack;
typedef struct Animation Animation;
typedef struct AnimSkeleton AnimSkeleton;

/**
animation system works with bone space poses (local)
render system works with character space poses (global)

anim track, animations -> local space

apply to skinned mesh -> local to global space

 **/
struct AnimSkeleton
{
	uint32_t id;
	uint32_t bones_length;
	uint32_t bones_identifier[MAX_BONES_PER_MESH];
	Float3x4 bones_local_transforms[MAX_BONES_PER_MESH]; // bone space
	Float3x4 bones_global_transforms[MAX_BONES_PER_MESH]; // character space
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
	uint32_t bones_length;
	Float3x4 local_transforms[MAX_BONES_PER_MESH]; // bone space
	Float3x4 global_transforms[MAX_BONES_PER_MESH]; // character space
	enum AnimPoseState state;
};

struct SkeletalMeshAsset;
struct SkeletalMeshInstance
{
	uint32_t render_bone_from_anim_bone[MAX_BONES_PER_MESH]; // mapping from anim skeleton to render skeleton
	Float3x4 pose[MAX_BONES_PER_MESH]; // global_from_local * local_from_bind
	struct SkeletalMeshAsset *asset;
	uint32_t skeletal_mesh_render_handle;
};

void Serialize_AnimTrack(Serializer *serializer, AnimTrack *value);
void Serialize_Animation(Serializer *serializer, Animation *value);
void Serialize_AnimSkeleton(Serializer *serializer, AnimSkeleton *value);

void anim_evaluate_animation(struct AnimSkeleton *skeleton, Animation const* anim, struct AnimPose *out_pose, float t);
void anim_pose_compute_global_transforms(struct AnimSkeleton *skeleton, struct AnimPose *pose);

void skeletal_mesh_init(struct SkeletalMeshAsset *asset, struct SkeletalMeshInstance *instance, struct AnimSkeleton *skeleton);
void skeletal_mesh_apply_pose(struct SkeletalMeshInstance *instance, struct AnimPose *pose);
