#include "asset.h"

void Serialize_MaterialAsset(Serializer *serializer, MaterialAsset *value)
{
	SV_ADD(SV_INITIAL, Blob, vertex_shader_bytecode);
	SV_ADD(SV_INITIAL, Blob, pixel_shader_bytecode);
	SV_ADD(SV_INITIAL, uint32_t, render_pass_id);
}
