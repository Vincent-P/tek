#pragma once

typedef struct Asset Asset;
typedef struct MaterialAsset MaterialAsset;
typedef struct MaterialInstance MaterialInstance;

struct MaterialAsset
{
	void *vertex_shader_bytecode;
	void *pixel_shader_bytecode;
	uint32_t vertex_shader_bytecode_length;
	uint32_t pixel_shader_bytecode_length;
	uint32_t render_pass_id;
};

struct MaterialInstance
{
	uint32_t pso_handle;
};

MaterialAsset load_material(const char *relative_path);
