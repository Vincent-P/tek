#include "asset.h"

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value)
{
	SV_ADD(SV_INITIAL, Blob, vertex_shader_bytecode);
	SV_ADD(SV_INITIAL, Blob, pixel_shader_bytecode);
	SV_ADD(SV_INITIAL, uint32_t, render_pass_id);
}

void Serialize_AnimTrack(Serializer *serializer, AnimTrack *value)
{
	SV_ADD(SV_INITIAL, Float3List, translations);
	SV_ADD(SV_INITIAL, QuatList, rotations);
	SV_ADD(SV_INITIAL, Float3List, scales);
}

void Serialize_Anim(Serializer *serializer, Anim *value)
{
	SV_ADD(SV_INITIAL, AnimTrack, root_motion_track);
	SV_ADD(SV_INITIAL, uint32_t, tracks_length);
	for (uint32_t i = 0; i < value->tracks_length; ++i) {
		SV_ADD(SV_INITIAL, AnimTrack, tracks[i]);
	}
}

void Serialize_SkeletalMeshWithAnimationsAsset(Serializer *serializer, SkeletalMeshWithAnimationsAsset *value)
{
	SV_ADD(SV_INITIAL, uint32_t, animations_length);
	for (uint32_t i = 0; i < value->animations_length; ++i) {
		SV_ADD(SV_INITIAL, Anim, animations[i]);
	}
}
