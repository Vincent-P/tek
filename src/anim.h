#pragma once

#define MAX_BONES_PER_MESH 64
#define MAX_ANIMATIONS_PER_ASSET 64

#define AssetId uint32_t
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
	AssetId id;
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
	AssetId id;
	AssetId skeleton_id;
	struct AnimTrack root_motion_track;
	struct AnimTrack tracks[MAX_BONES_PER_MESH];
	uint32_t tracks_identifier[MAX_BONES_PER_MESH]; // debug only
	uint32_t tracks_length;
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
	AnimSkeleton const* anim_skeleton;
	uint32_t bones_length;
	enum AnimPoseState state;
	Float3x4 local_transforms[MAX_BONES_PER_MESH]; // bone space
	Float3x4 global_transforms[MAX_BONES_PER_MESH]; // character space
};

#define MAX_WEIGHTS  4
typedef struct SkeletalMeshAsset SkeletalMeshAsset;
struct SkeletalMeshAsset
{
	AssetId id;
	uint32_t *indices;
	uint32_t indices_length;
	uint32_t vertices_length;
	Float3 *vertices_positions;
	Float3 *vertices_normals;
	Float3 *vertices_tangents;
	Float2 *vertices_uvs;
	uint32_t *vertices_colors;
	uint32_t *vertices_bone_indices;
	uint32_t *vertices_bone_weights;
	uint32_t bones_identifier[MAX_BONES_PER_MESH];
	Float3x4 bones_local_from_bind[MAX_BONES_PER_MESH]; // inverse bind pose
	uint32_t bones_length;
	uint32_t render_handle;
};
	
struct SkeletalMeshInstance
{
	uint32_t render_bone_from_anim_bone[MAX_BONES_PER_MESH]; // mapping from anim skeleton to render skeleton
	Float3x4 pose[MAX_BONES_PER_MESH]; // global_from_local * local_from_bind
	struct SkeletalMeshAsset const *asset;
};

void Serialize_AnimTrack(Serializer *serializer, AnimTrack *value);
void Serialize_Animation(Serializer *serializer, Animation *value);
void Serialize_AnimSkeleton(Serializer *serializer, AnimSkeleton *value);
void Serialize_SkeletalMeshAsset(Serializer *serializer, SkeletalMeshAsset *value);

void anim_evaluate_animation(struct AnimSkeleton const *skeleton, Animation const* anim, struct AnimPose *out_pose, float t);
void anim_pose_compute_global_transforms(struct AnimSkeleton const *skeleton, struct AnimPose *pose);

void skeletal_mesh_create_instance(struct SkeletalMeshAsset const *asset, struct SkeletalMeshInstance *instance, struct AnimSkeleton const *skeleton);
void skeletal_mesh_apply_pose(struct SkeletalMeshInstance *instance, struct AnimPose *pose);
