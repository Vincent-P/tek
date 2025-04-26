#pragma once
#include "serializer.h"
#include "maths.h"
#include "anim.h"

#define AssetId uint32_t

typedef struct MaterialAsset MaterialAsset;
struct MaterialAsset
{
	struct Blob vertex_shader_bytecode;
	struct Blob pixel_shader_bytecode;
	uint32_t render_pass_id;
	AssetId id;
};

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value);
;

typedef struct SkeletalMeshWithAnimationsAsset SkeletalMeshWithAnimationsAsset;
struct SkeletalMeshWithAnimationsAsset
{
	struct AnimSkeleton anim_skeleton;
	struct SkeletalMeshAsset skeletal_mesh;
	struct Animation animations[MAX_ANIMATIONS_PER_ASSET];
	uint32_t animations_length;
};
void Serialize_SkeletalMeshWithAnimationsAsset(Serializer *serializer, SkeletalMeshWithAnimationsAsset *value);

#define ASSET_MATERIAL_CAPACITY 4
#define ASSET_SKELETAL_MESH_CAPACITY 4
#define ASSET_ANIM_SKELETON_CAPACITY 4
#define ASSET_ANIMATIONS_CAPACITY 4

struct AssetLibrary
{
	MaterialAsset materials[ASSET_MATERIAL_CAPACITY];
	AssetId materials_id[ASSET_MATERIAL_CAPACITY];
	SkeletalMeshAsset skeletal_meshes[ASSET_SKELETAL_MESH_CAPACITY];
	AssetId skeletal_meshes_id[ASSET_SKELETAL_MESH_CAPACITY];
	AnimSkeleton anim_skeletons[ASSET_ANIM_SKELETON_CAPACITY];
	AssetId anim_skeletons_id[ASSET_ANIM_SKELETON_CAPACITY];
	Animation animations[ASSET_ANIMATIONS_CAPACITY];
	AssetId animations_id[ASSET_ANIMATIONS_CAPACITY];

	AssetId material_generation;
	AssetId skeletal_mesh_generation;
	AssetId anim_skeleton_generation;
	AssetId animation_generation;
};

MaterialAsset const *asset_library_get_material(struct AssetLibrary *assets, AssetId id);
SkeletalMeshAsset const *asset_library_get_skeletal_mesh(struct AssetLibrary *assets, AssetId id);
AnimSkeleton const *asset_library_get_anim_skeleton(struct AssetLibrary *assets, AssetId id);
Animation const *asset_library_get_animation(struct AssetLibrary *assets, AssetId id);
AssetId asset_library_add_material(struct AssetLibrary *assets, struct MaterialAsset material);
AssetId asset_library_add_skeletal_mesh(struct AssetLibrary *assets, struct SkeletalMeshAsset skeletal_mesh);
AssetId asset_library_add_anim_skeleton(struct AssetLibrary *assets, struct AnimSkeleton anim_skeleton);
AssetId asset_library_add_animation(struct AssetLibrary *assets, struct Animation animation);
