#include "anim.h"

void Serialize_AnimSkeleton(Serializer *serializer, AnimSkeleton *value)
{
	SV_ADD(SV_INITIAL, AssetId, id);
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
	SV_ADD(SV_INITIAL, AssetId, id);
	SV_ADD(SV_INITIAL, AssetId, skeleton_id);
	SV_ADD(SV_INITIAL, AnimTrack, root_motion_track);
	SV_ADD(SV_INITIAL, uint32_t, tracks_length);
	for (uint32_t i = 0; i < value->tracks_length; ++i) {
		SV_ADD(SV_INITIAL, AnimTrack, tracks[i]);
	}
	for (uint32_t i = 0; i < value->tracks_length; ++i) {
		SV_ADD(SV_INITIAL, uint32_t, tracks_identifier[i]);
	}
}

void Serialize_SkeletalMeshAsset(Serializer *serializer, SkeletalMeshAsset *value)
{
	SV_ADD(SV_INITIAL, AssetId, id);
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

bool anim_evaluate_animation(struct AnimSkeleton const *skeleton, Animation const* anim, struct AnimPose *out_pose, float t)
{
	uint32_t count = anim->tracks[0].translations.length;
	uint32_t iframe = (uint32_t)t;
	bool has_ended = iframe >= count;

	if (iframe >= count) {
		iframe = count - 1;
	}
	out_pose->bones_length = skeleton->bones_length;
	for (uint32_t ibone = 0; ibone < anim->tracks_length; ++ibone) {
		assert(anim->tracks[ibone].translations.length == count);
		assert(anim->tracks[ibone].rotations.length == count);
		assert(anim->tracks[ibone].scales.length == count);
		assert(anim->tracks_identifier[ibone] == skeleton->bones_identifier[ibone]);

		Float3 translation = anim->tracks[ibone].translations.data[iframe];
		Quat rotation = anim->tracks[ibone].rotations.data[iframe];
		Float3 scale = anim->tracks[ibone].scales.data[iframe];
		
		out_pose->local_transforms[ibone] = float3x4_from_transform(translation, rotation, scale);
	}

	return has_ended;
}

void anim_pose_compute_global_transforms(struct AnimSkeleton const *skeleton, struct AnimPose *pose)
{
	pose->global_transforms[0] = float3x4_mul(skeleton->bones_global_transforms[0], pose->local_transforms[0]);
	for (uint32_t i = 1; i < skeleton->bones_length; ++i) {
		uint8_t iparent = skeleton->bones_parent[i];
		assert(iparent < skeleton->bones_length);
		pose->global_transforms[i] = float3x4_mul(pose->global_transforms[iparent], pose->local_transforms[i]);
	}
}

void skeletal_mesh_create_instance(struct SkeletalMeshAsset const *asset, struct SkeletalMeshInstance *instance, struct AnimSkeleton const *skeleton)
{
	instance->asset = asset;
	// create mapping from anim bone to render bone
	for (uint32_t ianimbone = 0; ianimbone < skeleton->bones_length; ++ianimbone) {
		instance->render_bone_from_anim_bone[ianimbone] = MAX_BONES_PER_MESH;		
		for (uint32_t irenderbone = 0; irenderbone < asset->bones_length; ++irenderbone) {
			if (asset->bones_identifier[irenderbone] == skeleton->bones_identifier[ianimbone]) {
				instance->render_bone_from_anim_bone[ianimbone] = irenderbone;
				break;
			}
		}
	}
	// apply bind pose
	for (uint32_t ianimbone = 0; ianimbone < skeleton->bones_length; ++ianimbone) {
		uint32_t irenderbone = instance->render_bone_from_anim_bone[ianimbone];
		if (irenderbone < instance->asset->bones_length) {
			instance->pose[irenderbone] = float3x4_mul(
								   skeleton->bones_global_transforms[ianimbone],
								   instance->asset->bones_local_from_bind[irenderbone]
								   );
		}
	}

}

void skeletal_mesh_apply_pose(struct SkeletalMeshInstance *instance, struct AnimPose *pose)
{
	for (uint32_t ianimbone = 0; ianimbone < pose->bones_length; ++ianimbone) {
		uint32_t irenderbone = instance->render_bone_from_anim_bone[ianimbone];
		if (irenderbone < instance->asset->bones_length) {
			instance->pose[irenderbone] = float3x4_mul(
								   pose->global_transforms[ianimbone],
								   instance->asset->bones_local_from_bind[irenderbone]
								   );
		}
	}
}
