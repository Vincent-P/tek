#include "anim.h"

void anim_evaluate_animation(struct AnimSkeleton *skeleton, Animation const* anim, struct AnimPose *out_pose, float t)
{
	uint32_t count = anim->tracks[0].translations.length;
	uint32_t iframe = (uint32_t)t % count;
	
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

void anim_pose_compute_global_transforms(struct AnimSkeleton *skeleton, struct AnimPose *pose, Float3x4 root_world_transform)
{
	Float3x4 root_to_world = float3x4_mul(root_world_transform, skeleton->bones_global_transforms[0]);
	pose->global_transforms[0] = float3x4_mul(root_to_world, pose->local_transforms[0]);
	
	for (uint32_t i = 1; i < skeleton->bones_length; ++i) {
		uint8_t iparent = skeleton->bones_parent[i];
		assert(iparent < skeleton->bones_length);
		pose->global_transforms[i] = float3x4_mul(pose->global_transforms[iparent], pose->local_transforms[i]);
	}
}
