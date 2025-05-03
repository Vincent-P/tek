#include "asset.h"
#include "anim.h"

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value)
{
	SV_ADD(SV_INITIAL, Blob, vertex_shader_bytecode);
	SV_ADD(SV_INITIAL, Blob, pixel_shader_bytecode);
	SV_ADD(SV_INITIAL, uint32_t, render_pass_id);
	SV_ADD(SV_INITIAL, AssetId, id);
}


void Serialize_SkeletalMeshWithAnimationsAsset(Serializer *serializer, SkeletalMeshWithAnimationsAsset *value)
{
	SV_ADD(SV_INITIAL, AnimSkeleton, anim_skeleton);
	SV_ADD(SV_INITIAL, SkeletalMeshAsset, skeletal_mesh);
	SV_ADD(SV_INITIAL, uint32_t, animations_length);
	for (uint32_t i = 0; i < value->animations_length; ++i) {
		SV_ADD(SV_INITIAL, Animation, animations[i]);
	}
}

MaterialAsset const *asset_library_get_material(struct AssetLibrary *assets, AssetId id)
{
	for (uint32_t i = 0; i < ASSET_MATERIAL_CAPACITY; ++i) {
		if (assets->materials_id[i] == id) {
			return assets->materials + i;
		}
	}
	assert(false);
	return NULL;
}

SkeletalMeshAsset const *asset_library_get_skeletal_mesh(struct AssetLibrary *assets, AssetId id)
{
	for (uint32_t i = 0; i < ASSET_SKELETAL_MESH_CAPACITY; ++i) {
		if (assets->skeletal_meshes_id[i] == id) {
			return assets->skeletal_meshes + i;
		}
	}
	assert(false);
	return NULL;
}

AnimSkeleton const *asset_library_get_anim_skeleton(struct AssetLibrary *assets, AssetId id)
{
	for (uint32_t i = 0; i < ASSET_ANIM_SKELETON_CAPACITY; ++i) {
		if (assets->anim_skeletons_id[i] == id) {
			return assets->anim_skeletons + i;
		}
	}
	assert(false);
	return NULL;
}

Animation const *asset_library_get_animation(struct AssetLibrary *assets, AssetId id)
{
	for (uint32_t i = 0; i < ASSET_ANIMATIONS_CAPACITY; ++i) {
		if (assets->animations_id[i] == id) {
			return assets->animations + i;
		}
	}
	assert(false);
	return NULL;
}

AssetId asset_library_add_material(struct AssetLibrary *assets, struct MaterialAsset material)
{
	// support hot reload
	for (uint32_t i = 0;  i  < ASSET_MATERIAL_CAPACITY; ++i) {
		if (assets->materials_id[i] == material.id) {
			assets->materials[i] = material;
			return material.id;
		}
	}
	
	uint32_t i = assets->material_generation++;
	assert(i < ASSET_MATERIAL_CAPACITY);
	assets->materials[i] = material;
	assets->materials_id[i] = material.id;
	return material.id;
}

AssetId asset_library_add_skeletal_mesh(struct AssetLibrary *assets, struct SkeletalMeshAsset skeletal_mesh)
{
	uint32_t i = assets->skeletal_mesh_generation++;
	assert(i < ASSET_SKELETAL_MESH_CAPACITY);
	assets->skeletal_meshes[i] = skeletal_mesh;
	assets->skeletal_meshes_id[i] = skeletal_mesh.id;
	return skeletal_mesh.id;
}

AssetId asset_library_add_anim_skeleton(struct AssetLibrary *assets, struct AnimSkeleton anim_skeleton)
{
	uint32_t i = assets->anim_skeleton_generation++;
	assert(i < ASSET_ANIM_SKELETON_CAPACITY);
	assets->anim_skeletons[i] = anim_skeleton;
	assets->anim_skeletons_id[i] = anim_skeleton.id;
	return anim_skeleton.id;
}

AssetId asset_library_add_animation(struct AssetLibrary *assets, struct Animation animation)
{
	uint32_t i = assets->animation_generation++;
	assert(i < ASSET_ANIMATIONS_CAPACITY);
	assets->animations[i] = animation;
	assets->animations_id[i] = animation.id;
	return animation.id;
}
