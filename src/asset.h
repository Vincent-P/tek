#pragma once
#include "serializer.h"
#include "maths.h"

typedef struct MaterialAsset MaterialAsset;
typedef struct MaterialInstance MaterialInstance;
struct MaterialAsset
{
	struct Blob vertex_shader_bytecode;
	struct Blob pixel_shader_bytecode;
	uint32_t render_pass_id;
};

struct MaterialInstance
{
	uint32_t pso_handle;
};

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value);


#define MAX_BONES_PER_MESH 64
#define MAX_ANIMATIONS_PER_ASSET 64

typedef struct AnimTrack AnimTrack;
typedef struct Anim Anim;
typedef struct SkeletalMeshWithAnimationsAsset SkeletalMeshWithAnimationsAsset;
struct AnimTrack
{
	struct Float3List translations;
	struct QuatList rotations;
	struct Float3List scales;
};

struct Anim
{
	struct AnimTrack root_motion_track;
	struct AnimTrack tracks[MAX_BONES_PER_MESH];
	uint32_t tracks_length;
};


/**

animation skeleton:
  anim bones identifiers
  anim bones parent
  anim bones local

animation:
  bone_identifiers (per bone)
  tracks (per bone)
    positions[]
    rotations[]
    scales[]
  

skeletal mesh:
  vertices_position
  ...
  vertices_bone_indices
  vertices_bone_weights
  indices
  mesh bones geometry_to_bone (bind pose)
  mesh bones identifiers
  

RUNTIME
======
animation -> pose

Pose:
  skeleton ref
  local transforms
  global transforms (cached)
  state (unset, pose, reference, zero, additive)
  
runtime anim skeleton:
  skeleton ref
  anim bones world

the runtime anim skeleton is used to compute a pose, applied  to all character meshes
  
runtime skeletal mesh:
  mesh bones in world space

   
 **/
struct SkeletalMeshWithAnimationsAsset
{
	struct Anim animations[MAX_ANIMATIONS_PER_ASSET];
	uint32_t animations_length;
	
	Float3x4 bind_pose_bone_to_world[MAX_BONES_PER_MESH];
};
void Serialize_AnimTrack(Serializer *serializer, AnimTrack *value);
void Serialize_Anim(Serializer *serializer, Anim *value);
void Serialize_SkeletalMeshWithAnimationsAsset(Serializer *serializer, SkeletalMeshWithAnimationsAsset *value);
