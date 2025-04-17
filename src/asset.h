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


typedef struct SkeletalMeshWithAnimationsAsset SkeletalMeshWithAnimationsAsset;
struct SkeletalMeshWithAnimationsAsset
{
	struct AnimSkeleton anim_skeleton;
	
	struct Animation animations[MAX_ANIMATIONS_PER_ASSET];
	uint32_t animations_length;
};
void Serialize_SkeletalMeshWithAnimationsAsset(Serializer *serializer, SkeletalMeshWithAnimationsAsset *value);
