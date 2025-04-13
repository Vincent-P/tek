#pragma once
#include "serializer.h"

typedef struct Asset Asset;
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
