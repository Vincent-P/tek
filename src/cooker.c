#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define XXH_INLINE_ALL
#include <xxhash.h>
#include <json.h>
#include <shaderc/shaderc.h>
#include <ufbx.h>

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(*x))

#include "asset.h"
#include "file.h"
#include "vulkan.h" // for RENDER_PASSES

const char *asset_type;
const char *relative_path;
const char *source_dir;
const char *cooking_dir;

int cook_material()
{
	char source_path[512];
	char dep_path[512];
	snprintf(source_path, sizeof(source_path), "%s%s", source_dir, relative_path);
	snprintf(dep_path, sizeof(dep_path), "%s%s.dep", cooking_dir, relative_path);
	
	// Parse JSON
	char vertex_shader_path[512] = {0};
	char pixel_shader_path[512] = {0};
	uint32_t render_pass_id = ~0u;
	struct Blob material_json_file = file_read_entire_file(source_path);
	struct json_value_s* root = json_parse_ex(material_json_file.data, material_json_file.size, json_parse_flags_allow_trailing_comma , NULL, NULL, NULL);
	assert(root->type == json_type_object);
	struct json_object_s* object = (struct json_object_s*)root->payload;
	for (struct json_object_element_s* it = object->start; it != NULL; it = it->next) {
		if (strcmp(it->name->string, "vertex_shader") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			snprintf(vertex_shader_path, sizeof(vertex_shader_path), "%s%s", source_dir, value->string);
		}
		if (strcmp(it->name->string, "pixel_shader") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			snprintf(pixel_shader_path, sizeof(pixel_shader_path), "%s%s", source_dir, value->string);
		}
		if (strcmp(it->name->string, "render_pass") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			for (uint32_t ipass = 0; ipass < RENDER_PASSES_COUNT; ++ipass) {
				if (strcmp(RENDER_PASSES[ipass].name, value->string) == 0) {
					render_pass_id = ipass;
					break;
				}
			}
			assert(render_pass_id < RENDER_PASSES_COUNT);
		}
	}

	// Compile shaders
	struct Blob vshader_file = file_read_entire_file(vertex_shader_path);
	struct Blob pshader_file = file_read_entire_file(pixel_shader_path);
	
	shaderc_compiler_t shader_compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	shaderc_compile_options_set_generate_debug_info(options);
	
	shaderc_compilation_result_t vshader_result = shaderc_compile_into_spv(shader_compiler,
								       vshader_file.data, vshader_file.size,
								       shaderc_vertex_shader, vertex_shader_path, "main", options);
	if (shaderc_result_get_compilation_status(vshader_result) != shaderc_compilation_status_success) {
		size_t num_warnings = shaderc_result_get_num_warnings(vshader_result);
		size_t num_errors = shaderc_result_get_num_errors(vshader_result);
		const char* error_msg = shaderc_result_get_error_message(vshader_result);
		printf("%zu warnings. %zu errors.\n%s\n", num_warnings, num_errors, error_msg);
		return 1;
	}

	shaderc_compilation_result_t pshader_result = shaderc_compile_into_spv(shader_compiler,
									       pshader_file.data, pshader_file.size,
									       shaderc_fragment_shader, pixel_shader_path, "main", options);
	if (shaderc_result_get_compilation_status(pshader_result) != shaderc_compilation_status_success) {
		size_t num_warnings = shaderc_result_get_num_warnings(pshader_result);
		size_t num_errors = shaderc_result_get_num_errors(pshader_result);
		const char* error_msg = shaderc_result_get_error_message(pshader_result);
		printf("%zu warnings. %zu errors.\n%s\n", num_warnings, num_errors, error_msg);
		return 1;
	}

	// Save material to disk
	struct MaterialAsset material = {0};
	
	material.vertex_shader_bytecode.size = shaderc_result_get_length(vshader_result);
	material.vertex_shader_bytecode.data = (char*)shaderc_result_get_bytes(vshader_result);
	material.pixel_shader_bytecode.size = shaderc_result_get_length(pshader_result);
	material.pixel_shader_bytecode.data = (char*)shaderc_result_get_bytes(pshader_result);
	material.render_pass_id = render_pass_id;

	uint32_t serializer_capacity = (4 << 10) + material.vertex_shader_bytecode.size + material.pixel_shader_bytecode.size;
	Serializer serializer = serialize_begin_write_file(serializer_capacity);
	Serialize_MaterialAsset(&serializer, &material);

	XXH64_hash_t hash = XXH64(serializer.buffer.data, serializer.cursor, 0);
	char dest_path[512];
	snprintf(dest_path, sizeof(dest_path), "%s%llx", cooking_dir, hash);
	serialize_end_write_file(&serializer, dest_path);

	// Save dep file to disk
	char dep_content[512] = {0};
	int32_t dep_cursor = 0;
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", vertex_shader_path);
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", pixel_shader_path);
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "OUTPUT: %s\n", dest_path);
	file_write_entire_file(dep_path, (struct Blob){dep_content, dep_cursor});

	return 0;
}

int cook_fbx()
{
	char source_path[512];
	char dep_path[512];
	snprintf(source_path, sizeof(source_path), "%s%s", source_dir, relative_path);
	snprintf(dep_path, sizeof(dep_path), "%s%s.dep", cooking_dir, relative_path);

	ufbx_load_opts opts = { 0 }; // Optional, pass NULL for defaults
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.target_unit_meters = 1.0f;
	
	ufbx_error error; // Optional, pass NULL if you don't care about errors
	ufbx_scene *scene = ufbx_load_file(source_path, &opts, &error);
	if (!scene) {
		fprintf(stderr, "Failed to load: %s\n", error.description.data);
		return 1;
	}

	// Use and inspect `scene`, it's just plain data!

	// Let's just list all objects within the scene for example:
	ufbx_node *root_bone = NULL;
	for (size_t i = 0; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		if (node->mesh) {
			printf("Mesh: %s\n", node->name.data);
			printf("\t faces: %zu\n", node->mesh->faces.count);
		} else if (node->bone) {
			bool is_root_bone = node->bone->is_root || node->parent->bone == NULL;
			printf("%s Bone: %s\n", is_root_bone ? "Root " : "", node->name.data);

			if (is_root_bone) {
				assert(root_bone == NULL);
				root_bone = node;
			}
			
		} else {
			printf("Node: %s\n", node->name.data);
		}
	}

	if (scene->skin_deformers.count > 0) {
		ufbx_skin_deformer *deformer = scene->skin_deformers.data[0];
		printf("Deformer: %s\n", deformer->name.data);
		const char* method = NULL;
		if (deformer->skinning_method == UFBX_SKINNING_METHOD_LINEAR) {
			method = "linear";
		} else if (deformer->skinning_method == UFBX_SKINNING_METHOD_RIGID) {
			method = "rigid";
		} else if (deformer->skinning_method == UFBX_SKINNING_METHOD_DUAL_QUATERNION) {
			method = "dual quaternion";
		} else if (deformer->skinning_method == UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR) {
			method = "blended dq linear";
		}
		printf("\tmethod: %s\n", method);
		printf("\tmax weights per vertex: %zu\n", deformer->max_weights_per_vertex);
		printf("\tbones: %zu\n", deformer->clusters.count);
		for (size_t icluster = 0; icluster < deformer->clusters.count; ++icluster) {
			ufbx_skin_cluster *cluster = deformer->clusters.data[icluster];
			printf("\t\tCluster: %s (%zu)\n", cluster->name.data, cluster->num_weights);
		}
	}

	printf("Poses: %zu\n", scene->poses.count);
	for (size_t ipose = 0; ipose < scene->poses.count; ipose++) {
		ufbx_pose *pose = scene->poses.data[ipose];
		printf("\tis bind pose: %s\n", pose->is_bind_pose ? "true": "false");
		printf("\tbones: %zu\n", pose->bone_poses.count);
	}
	
	for (size_t istack = 0; istack < scene->anim_stacks.count && false; istack++) {
		ufbx_anim_stack *stack = scene->anim_stacks.data[istack];
		ufbx_anim *anim = stack->anim;
		printf("Anim stack: %s\n", stack->name.data);

		ufbx_bake_opts opts = {0};
		opts.key_reduction_enabled = false;
		ufbx_baked_anim *bake = ufbx_bake_anim(scene, anim, &opts, NULL);
		assert(bake);

		printf("\tbones: %zu\n", bake->nodes.count);
		for (size_t inode = 0; inode < bake->nodes.count; inode++) {
			ufbx_baked_node *baked_node = &bake->nodes.data[inode];
			ufbx_node *scene_node = scene->nodes.data[baked_node->typed_id];

			printf("\t  node %s: t[%zu]%u r[%zu]%u s[%zu]%u\n",
			       scene_node->name.data,
			       baked_node->translation_keys.count,
			       (uint32_t)baked_node->constant_translation,
			       baked_node->rotation_keys.count,
			       (uint32_t)baked_node->constant_rotation,
			       baked_node->scale_keys.count,
			       (uint32_t)baked_node->constant_scale);
		}

		ufbx_free_baked_anim(bake);
	}


	struct SkeletalMeshWithAnimationsAsset skeletal_mesh_with_animations = {0};
	skeletal_mesh_with_animations.animations_length = scene->anim_stacks.count;
	
	for (size_t istack = 0; istack < scene->anim_stacks.count; istack++) {
		ufbx_anim_stack *stack = scene->anim_stacks.data[istack];
		ufbx_anim *anim = stack->anim;
		ufbx_bake_opts opts = {0};
		opts.key_reduction_enabled = true;
		ufbx_baked_anim *bake = ufbx_bake_anim(scene, anim, &opts, NULL);
		assert(bake);

		struct Anim *out_anim = skeletal_mesh_with_animations.animations + istack;
		out_anim->tracks_length = bake->nodes.count;

		for (size_t inode = 0; inode < bake->nodes.count; inode++) {
			ufbx_baked_node *baked_node = &bake->nodes.data[inode];
			ufbx_node *scene_node = scene->nodes.data[baked_node->typed_id];
			
			struct AnimTrack *out_track = out_anim->tracks + inode;
			out_track->translations.length = baked_node->translation_keys.count;
			out_track->translations.data = calloc(baked_node->translation_keys.count, sizeof(Float3));
			for (uint32_t i = 0; i < out_track->translations.length; ++i) {
				out_track->translations.data[i].x = baked_node->translation_keys.data[i].value.x;
				out_track->translations.data[i].y = baked_node->translation_keys.data[i].value.y;
				out_track->translations.data[i].z = baked_node->translation_keys.data[i].value.z;
			}
			
			out_track->rotations.length = baked_node->rotation_keys.count;
			out_track->rotations.data = calloc(baked_node->rotation_keys.count, sizeof(Quat));
			for (uint32_t i = 0; i < out_track->rotations.length; ++i) {
				out_track->rotations.data[i].x = baked_node->rotation_keys.data[i].value.x;
				out_track->rotations.data[i].y = baked_node->rotation_keys.data[i].value.y;
				out_track->rotations.data[i].z = baked_node->rotation_keys.data[i].value.z;
				out_track->rotations.data[i].w = baked_node->rotation_keys.data[i].value.w;
			}

			out_track->scales.length = baked_node->scale_keys.count;
			out_track->scales.data = calloc(baked_node->scale_keys.count, sizeof(Float3));
			for (uint32_t i = 0; i < out_track->scales.length; ++i) {
				out_track->scales.data[i].x = baked_node->scale_keys.data[i].value.x;
				out_track->scales.data[i].y = baked_node->scale_keys.data[i].value.y;
				out_track->scales.data[i].z = baked_node->scale_keys.data[i].value.z;
			}
		}
		
		ufbx_free_baked_anim(bake);
	}

	ufbx_free_scene(scene);

	uint32_t serializer_capacity = (16 << 20);
	Serializer serializer = serialize_begin_write_file(serializer_capacity);
	Serialize_SkeletalMeshWithAnimationsAsset(&serializer, &skeletal_mesh_with_animations);

	XXH64_hash_t hash = XXH64(serializer.buffer.data, serializer.cursor, 0);
	char dest_path[512];
	snprintf(dest_path, sizeof(dest_path), "%s%llx", cooking_dir, hash);
	serialize_end_write_file(&serializer, dest_path);

	// Save dep file to disk
	char dep_content[512] = {0};
	int32_t dep_cursor = 0;
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", source_path);
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "OUTPUT: %s\n", dest_path);
	file_write_entire_file(dep_path, (struct Blob){dep_content, dep_cursor});
	
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 5) {
		printf("got %d arguments, expected 4\n", argc);
		return 1;
	}

	asset_type = argv[1];
	relative_path = argv[2];
	source_dir = argv[3];
	cooking_dir = argv[4];
	
	fprintf(stderr, "asset_type: %s\n", asset_type);
	fprintf(stderr, "relative_path: %s\n", relative_path);
	fprintf(stderr, "source_dir: %s\n", source_dir);
	fprintf(stderr, "cooking_dir: %s\n", cooking_dir);

	if (strcmp(asset_type, "mat") == 0) {
		return cook_material();
	} else if (strcmp(asset_type, "fbx") == 0) {
		return cook_fbx();
	}
	
	return 1;
}

#include "asset.c"
#include <ufbx.c>
