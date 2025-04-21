#include "anim.h"

void anim_evaluate_animation(struct AnimSkeleton *skeleton, Animation const* anim, struct AnimPose *out_pose, float t)
{
	uint32_t count = anim->tracks[0].translations.length;
	uint32_t iframe = (uint32_t)t % count;

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
}

void anim_pose_compute_global_transforms(struct AnimSkeleton *skeleton, struct AnimPose *pose)
{
	pose->global_transforms[0] = float3x4_mul(skeleton->bones_global_transforms[0], pose->local_transforms[0]);
	for (uint32_t i = 1; i < skeleton->bones_length; ++i) {
		uint8_t iparent = skeleton->bones_parent[i];
		assert(iparent < skeleton->bones_length);
		pose->global_transforms[i] = float3x4_mul(pose->global_transforms[iparent], pose->local_transforms[i]);
	}
}

void skeletal_mesh_init(struct SkeletalMeshAsset *asset, struct SkeletalMeshInstance *instance, struct AnimSkeleton *skeleton)
{
	instance->asset = asset;
	
	for (uint32_t ianimbone = 0; ianimbone < skeleton->bones_length; ++ianimbone) {
		instance->render_bone_from_anim_bone[ianimbone] = MAX_BONES_PER_MESH;
		
		for (uint32_t irenderbone = 0; irenderbone < asset->bones_length; ++irenderbone) {
			if (asset->bones_identifier[irenderbone] == skeleton->bones_identifier[ianimbone]) {
				instance->render_bone_from_anim_bone[ianimbone] = irenderbone;
				break;
			}
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
