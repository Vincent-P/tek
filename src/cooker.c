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
	XXH64_hash_t hash = XXH64(relative_path, strlen(relative_path), 0);
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

uint32_t ufbx_string_to_id(ufbx_string s)
{
	XXH64_hash_t hash = XXH64(s.data, s.length, 0);
	return (uint32_t)hash;
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

	// Import skeleton from the nodes hierarchy
	ufbx_node *root_bone = NULL;
	ufbx_pose *bind_pose = NULL;
	ufbx_node *bones[MAX_BONES_PER_MESH] = {0};
	uint32_t bones_length = 0;
	uint32_t scene_bone_count = 0;
	for (size_t i = 0; i < scene->nodes.count; i++) {
		if (scene->nodes.data[i]->bone != NULL) {
			scene_bone_count += 1;
		}
	}
	for (size_t i = 0; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		if (root_bone != NULL && node->node_depth <= root_bone->node_depth) {
			break;
		}
		if (node->bone) {
			bool is_root_bone = node->bone->is_root || node->parent->bone == NULL;
			if (is_root_bone) {
				assert(root_bone == NULL);
				assert(node->bind_pose != NULL);
				root_bone = node;
				bind_pose = node->bind_pose;
			} else {
				assert(root_bone != NULL);
			}
			assert(bones_length < MAX_BONES_PER_MESH);
			bones[bones_length++] = node;
			fprintf(stderr, "Importing bone %s\n", node->name.data);
		}
	}
	fprintf(stderr, "Imported %u bones\n", bones_length);
	assert(bones_length == scene_bone_count);
	uint8_t bones_parent[MAX_BONES_PER_MESH] = {0};
	for (uint32_t ibone = 1; ibone < bones_length; ++ibone) {
		uint32_t iparent = 0;
		for (; iparent < ibone; ++iparent) {
			if (bones[iparent] == bones[ibone]->parent) {
				break;
			}
		}
		assert(iparent < ibone);
		bones_parent[ibone] = iparent;
	}

	// Import animations
	struct Animation animations[MAX_ANIMATIONS_PER_ASSET];
	assert(scene->anim_stacks.count < MAX_ANIMATIONS_PER_ASSET);
	for (size_t istack = 0; istack < scene->anim_stacks.count; istack++) {
		ufbx_anim_stack *stack = scene->anim_stacks.data[istack];
		ufbx_anim *anim = stack->anim;
		ufbx_bake_opts opts = {0};
		opts.key_reduction_enabled = false;
		ufbx_baked_anim *bake = ufbx_bake_anim(scene, anim, &opts, NULL);
		assert(bake);
		fprintf(stderr, "Importing anim: %s\n", stack->name.data);
		for (size_t inode = 0; inode < bake->nodes.count; inode++) {
			ufbx_baked_node *baked_node = &bake->nodes.data[inode];
			ufbx_node *node = scene->nodes.data[baked_node->typed_id];
			// Find the matching bone in the skeleton. Skip if not found.
			uint32_t ibone = 0;
			for (; ibone < bones_length; ++ibone) {
				if (bones[ibone] == node) {
					break;
				}
			}
			if (ibone >= bones_length) {
				// fprintf(stderr, "error skipping baked node: %s\n", node->name.data);
				continue;
			}
			// fprintf(stderr, "baked node: %s importing at track %u\n", node->name.data, ibone);
			animations[istack].tracks_length += 1;
			animations[istack].tracks_identifier[ibone] = ufbx_string_to_id(node->name);
			animations[istack].tracks[ibone].translations.length = baked_node->translation_keys.count;
			animations[istack].tracks[ibone].translations.data = calloc(baked_node->translation_keys.count, sizeof(Float3));
			struct Float3List translations = animations[istack].tracks[ibone].translations;
			ufbx_baked_vec3_list baked_translations = baked_node->translation_keys;
			for (uint32_t itrans = 0; itrans < baked_node->translation_keys.count; ++itrans) {
				translations.data[itrans].x = baked_translations.data[itrans].value.x;
				translations.data[itrans].y = baked_translations.data[itrans].value.y;
				translations.data[itrans].z = baked_translations.data[itrans].value.z;
			}
			animations[istack].tracks[ibone].rotations.length = baked_node->rotation_keys.count;
			animations[istack].tracks[ibone].rotations.data = calloc(baked_node->rotation_keys.count, sizeof(Quat));
			struct QuatList rotations = animations[istack].tracks[ibone].rotations;
			ufbx_baked_quat_list baked_rotations = baked_node->rotation_keys;
			for (uint32_t itrans = 0; itrans < baked_node->rotation_keys.count; ++itrans) {
				rotations.data[itrans].x = baked_rotations.data[itrans].value.x;
				rotations.data[itrans].y = baked_rotations.data[itrans].value.y;
				rotations.data[itrans].z = baked_rotations.data[itrans].value.z;
				rotations.data[itrans].w = baked_rotations.data[itrans].value.w;
			}
			animations[istack].tracks[ibone].scales.length = baked_node->scale_keys.count;
			animations[istack].tracks[ibone].scales.data = calloc(baked_node->scale_keys.count, sizeof(Float3));
			struct Float3List scales = animations[istack].tracks[ibone].scales;
			ufbx_baked_vec3_list baked_scales = baked_node->scale_keys;
			for (uint32_t itrans = 0; itrans < baked_node->scale_keys.count; ++itrans) {
				scales.data[itrans].x = baked_scales.data[itrans].value.x;
				scales.data[itrans].y = baked_scales.data[itrans].value.y;
				scales.data[itrans].z = baked_scales.data[itrans].value.z;
			}
		}
		ufbx_free_baked_anim(bake);
	}

	// Import the character mesh
	assert(scene->meshes.count == 1);
	ufbx_mesh *mesh = scene->meshes.data[0];
	assert(mesh->skin_deformers.count == 1);
	ufbx_skin_deformer *skin = mesh->skin_deformers.data[0];
	assert(skin->clusters.count <= MAX_BONES_PER_MESH);
	uint32_t clusters_bone_identifier[MAX_BONES_PER_MESH] = {0};
	ufbx_matrix clusters_geometry_to_bone[MAX_BONES_PER_MESH] = {0};
	uint32_t clusters_length = skin->clusters.count;
	for (uint32_t icluster = 0; icluster < clusters_length; ++icluster) {
		ufbx_skin_cluster *cluster = skin->clusters.data[icluster];
		clusters_bone_identifier[icluster] = ufbx_string_to_id(cluster->bone_node->name);
		clusters_geometry_to_bone[icluster] = cluster->geometry_to_bone; // inverse bind matrix
		fprintf(stderr, "Importing mesh skinned bone %s\n", cluster->bone_node->name.data);
	}
	fprintf(stderr, "Imported %u mesh skinned bones\n", clusters_length);
	uint32_t mesh_indices_length = mesh->num_faces * 3;
	uint32_t mesh_vertices_length = mesh_indices_length; // TODO
	uint32_t *indices = calloc(mesh_indices_length, sizeof(uint32_t));
	Float3 *vertices_positions = calloc(mesh_vertices_length, sizeof(Float3));
	Float3 *vertices_normals = calloc(mesh_vertices_length, sizeof(Float3));
	Float3 *vertices_tangents = calloc(mesh_vertices_length, sizeof(Float3));
	Float2 *vertices_uvs = calloc(mesh_vertices_length, sizeof(Float2));
	uint32_t *vertices_colors = calloc(mesh_vertices_length, sizeof(uint32_t));
	uint32_t *vertices_bone_indices = calloc(mesh_vertices_length, sizeof(uint32_t));
	uint32_t *vertices_bone_weights = calloc(mesh_vertices_length, sizeof(uint32_t));
	uint32_t ivertex = 0;
	uint32_t iindex = 0;
	for (uint32_t iface = 0; iface < mesh->num_faces; ++iface) {
		ufbx_face face = mesh->faces.data[iface];
		assert(face.num_indices == 3);
		for (uint32_t corner = 0; corner < face.num_indices; ++corner) {
			uint32_t index = face.index_begin + corner;
			ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
			ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
			ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
			// ufbx_vec3 tangent = ufbx_get_vertex_vec3(&mesh->vertex_tangent, index);
			// ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh->vertex_color, index);
			indices[iindex++] = index;
			vertices_positions[ivertex].x = position.x;
			vertices_positions[ivertex].y = position.y;
			vertices_positions[ivertex].z = position.z;
			vertices_normals[ivertex].x = normal.x;
			vertices_normals[ivertex].y = normal.y;
			vertices_normals[ivertex].z = normal.z;
			vertices_tangents[ivertex] = (Float3){0};
			vertices_uvs[ivertex].x = uv.x;
			vertices_uvs[ivertex].y = uv.y;
			vertices_colors[ivertex] = 0xffffffffu;

			// Compute bone weights
			uint32_t final_bone_indices = 0;
			uint32_t final_bone_weights = 0;
			uint32_t vertex_bones_index[MAX_WEIGHTS] = {0};
			float vertex_bones_weight[MAX_WEIGHTS] = {0};
			uint32_t vertex_bones_normalized_weight[MAX_WEIGHTS] = {0};
			uint32_t vertex = mesh->vertex_indices.data[index];
			ufbx_skin_vertex skin_vertex = skin->vertices.data[vertex];
			assert(skin_vertex.num_weights < MAX_WEIGHTS);
			float total_weight = 0.0f;
			for (size_t i = 0; i < skin_vertex.num_weights; i++) {
				ufbx_skin_weight skin_weight = skin->weights.data[skin_vertex.weight_begin + i];
				vertex_bones_index[i] = skin_weight.cluster_index;
				vertex_bones_weight[i] = (float)skin_weight.weight;
				total_weight += (float)skin_weight.weight;
			}
			// FBX does not guarantee that skin weights are normalized, and we may even
			// be dropping some, so we must renormalize them.
			for (size_t i = 0; i < skin_vertex.num_weights; i++) {
				float w  = vertex_bones_weight[i] / total_weight;
				
				vertex_bones_normalized_weight[i] = (uint32_t)(w* 255.0f);
			}

			vertices_bone_indices[ivertex] = (vertex_bones_index[0] & 0xff)
				| ((vertex_bones_index[1] & 0xff) << 8)
				| ((vertex_bones_index[2] & 0xff) << 16)
				| ((vertex_bones_index[3] & 0xff) << 24);
			
			vertices_bone_weights[ivertex] = (vertex_bones_normalized_weight[0] & 0xff)
				| ((vertex_bones_normalized_weight[1] & 0xff) << 8)
				| ((vertex_bones_normalized_weight[2] & 0xff) << 16)
				| ((vertex_bones_normalized_weight[3] & 0xff) << 24);

			ivertex += 1;
		}
	}

	// Fill the Asset struct
	struct SkeletalMeshWithAnimationsAsset skeletal_mesh_with_animations = {0};
	skeletal_mesh_with_animations.animations_length = scene->anim_stacks.count;
	for (uint32_t ianim = 0; ianim < scene->anim_stacks.count; ++ianim) {
		skeletal_mesh_with_animations.animations[ianim] = animations[ianim];
	}
	skeletal_mesh_with_animations.anim_skeleton.bones_length = bones_length;
	for (uint32_t ibone = 0; ibone < bones_length; ++ibone) {
		ufbx_bone_pose *bone_pose = ufbx_get_bone_pose(bind_pose, bones[ibone]);
		assert(bone_pose != NULL);
		for (uint32_t i = 0; i < 12; ++i) {
			skeletal_mesh_with_animations.anim_skeleton.bones_local_transforms[ibone].values[i] = bone_pose->bone_to_parent.v[i];
		}
		for (uint32_t i = 0; i < 12; ++i) {
			skeletal_mesh_with_animations.anim_skeleton.bones_global_transforms[ibone].values[i] = bone_pose->bone_to_world.v[i];
		}
		skeletal_mesh_with_animations.anim_skeleton.bones_parent[ibone] = bones_parent[ibone];
		skeletal_mesh_with_animations.anim_skeleton.bones_identifier[ibone] = ufbx_string_to_id(bones[ibone]->name);
	}
	skeletal_mesh_with_animations.skeletal_mesh.indices = indices;
	skeletal_mesh_with_animations.skeletal_mesh.indices_length = mesh_indices_length;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_length = mesh_vertices_length;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_positions = vertices_positions;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_normals = vertices_normals;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_tangents = vertices_tangents;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_uvs = vertices_uvs;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_colors = vertices_colors;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_bone_indices = vertices_bone_indices;
	skeletal_mesh_with_animations.skeletal_mesh.vertices_bone_weights = vertices_bone_weights;
	skeletal_mesh_with_animations.skeletal_mesh.bones_length = clusters_length;
	for (uint32_t icluster = 0; icluster < clusters_length; ++icluster) {
		for (uint32_t i = 0; i < 12; ++i) {
			skeletal_mesh_with_animations.skeletal_mesh.bones_local_from_bind[icluster].values[i] = clusters_geometry_to_bone[icluster].v[i];
		}
	}
	for (uint32_t icluster = 0; icluster < clusters_length; ++icluster) {
		skeletal_mesh_with_animations.skeletal_mesh.bones_identifier[icluster] = clusters_bone_identifier[icluster];
	}
	
	ufbx_free_scene(scene);

	uint32_t serializer_capacity = (16 << 20);
	Serializer serializer = serialize_begin_write_file(serializer_capacity);
	Serialize_SkeletalMeshWithAnimationsAsset(&serializer, &skeletal_mesh_with_animations);
	XXH64_hash_t hash = XXH64(relative_path, strlen(relative_path), 0);
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
