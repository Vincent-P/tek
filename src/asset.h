#pragma once
#include "serializer.h"
#include "maths.h"
#include "anim.h"

typedef struct MaterialAsset MaterialAsset;
struct MaterialAsset
{
	struct Blob vertex_shader_bytecode;
	struct Blob pixel_shader_bytecode;
	uint32_t render_pass_id;
};

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value);

#define MAX_WEIGHTS  4
typedef struct SkeletalMeshAsset SkeletalMeshAsset;
struct SkeletalMeshAsset
{
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
};

typedef struct SkeletalMeshWithAnimationsAsset SkeletalMeshWithAnimationsAsset;
struct SkeletalMeshWithAnimationsAsset
{
	struct AnimSkeleton anim_skeleton;
	struct SkeletalMeshAsset skeletal_mesh;
	struct Animation animations[MAX_ANIMATIONS_PER_ASSET];
	uint32_t animations_length;
};
void Serialize_SkeletalMeshWithAnimationsAsset(Serializer *serializer, SkeletalMeshWithAnimationsAsset *value);
