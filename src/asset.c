#include "asset.h"

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value)
{
	SV_ADD(SV_INITIAL, Blob, vertex_shader_bytecode);
	SV_ADD(SV_INITIAL, Blob, pixel_shader_bytecode);
	SV_ADD(SV_INITIAL, uint32_t, render_pass_id);
}

void Serialize_AnimSkeleton(Serializer *serializer, AnimSkeleton *value)
{
	SV_ADD(SV_INITIAL, uint32_t, id);
	SV_ADD(SV_INITIAL, uint32_t, bones_length);
	for (uint32_t i = 0; i < value->bones_length; ++i) {
		SV_ADD(SV_INITIAL, uint32_t, bones_identifier[i]);
	}
	for (uint32_t i = 0; i < value->bones_length; ++i) {
		SV_ADD(SV_INITIAL, Float3x4, bones_local_transforms[i]);
	}
	for (uint32_t i = 0; i < value->bones_length; ++i) {
		SV_ADD(SV_INITIAL, Float3x4, bones_global_transforms[i]);
	}
	for (uint32_t i = 0; i < value->bones_length; ++i) {
		SV_ADD(SV_INITIAL, uint8_t, bones_parent[i]);
	}
}

void Serialize_AnimTrack(Serializer *serializer, AnimTrack *value)
{
	SV_ADD(SV_INITIAL, Float3List, translations);
	SV_ADD(SV_INITIAL, QuatList, rotations);
	SV_ADD(SV_INITIAL, Float3List, scales);
}

void Serialize_Animation(Serializer *serializer, Animation *value)
{
	SV_ADD(SV_INITIAL, AnimTrack, root_motion_track);
	SV_ADD(SV_INITIAL, uint32_t, tracks_length);
	for (uint32_t i = 0; i < value->tracks_length; ++i) {
		SV_ADD(SV_INITIAL, AnimTrack, tracks[i]);
	}
	for (uint32_t i = 0; i < value->tracks_length; ++i) {
		SV_ADD(SV_INITIAL, uint32_t, tracks_identifier[i]);
	}
	SV_ADD(SV_INITIAL, uint32_t, skeleton_id);
}

void Serialize_SkeletalMeshAsset(Serializer *serializer, SkeletalMeshAsset *value)
{
	SV_ADD(SV_INITIAL, uint32_t, indices_length);
	SV_ADD(SV_INITIAL, uint32_t, vertices_length);
	SV_ADD(SV_INITIAL, uint32_t, bones_length);
	if (serializer->is_reading) {
		value->indices = calloc(value->indices_length, sizeof(uint32_t));
		value->vertices_positions = calloc(value->vertices_length, sizeof(Float3));
		value->vertices_normals = calloc(value->vertices_length, sizeof(Float3));
		value->vertices_tangents = calloc(value->vertices_length, sizeof(Float3));
		value->vertices_uvs = calloc(value->vertices_length, sizeof(Float2));
		value->vertices_colors = calloc(value->vertices_length, sizeof(uint32_t));
		value->vertices_bone_indices = calloc(value->vertices_length, sizeof(uint32_t));
		value->vertices_bone_weights = calloc(value->vertices_length, sizeof(uint32_t));
	}
	if (serializer->version>=SV_INITIAL) {
		SerializeBytes(serializer, value->indices, value->indices_length*sizeof(uint32_t));
		SerializeBytes(serializer, value->vertices_positions, value->vertices_length*sizeof(Float3));
		SerializeBytes(serializer, value->vertices_normals, value->vertices_length*sizeof(Float3));
		SerializeBytes(serializer, value->vertices_tangents, value->vertices_length*sizeof(Float3));
		SerializeBytes(serializer, value->vertices_uvs, value->vertices_length*sizeof(Float2));
		SerializeBytes(serializer, value->vertices_colors, value->vertices_length*sizeof(uint32_t));
		SerializeBytes(serializer, value->vertices_bone_indices, value->vertices_length*sizeof(uint32_t));
		SerializeBytes(serializer, value->vertices_bone_weights, value->vertices_length*sizeof(uint32_t));
	}
	for (uint32_t i = 0; i < value->bones_length; ++i) {
		SV_ADD(SV_INITIAL, uint32_t, bones_identifier[i]);
	}
	for (uint32_t i = 0; i < value->bones_length; ++i) {
		SV_ADD(SV_INITIAL, Float3x4, bones_local_from_bind[i]);
	}
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
