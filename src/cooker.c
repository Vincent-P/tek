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

char* shader_includes_str[256];
int shader_includes_str_length[256];
uint32_t shader_includes_length;

#define COOKER_JSON_PARSE_FLAGS json_parse_flags_allow_c_style_comments | json_parse_flags_allow_trailing_comma

uint32_t string_to_id(const char *s, size_t l)
{
	XXH64_hash_t hash = XXH64(s, l, 0);
	return (uint32_t)hash;
}

uint32_t ufbx_string_to_id(ufbx_string s)
{
	return string_to_id(s.data, s.length);
}

// An includer callback type for mapping an #include request to an include
// result. The user_data parameter specifies the client context.  The
// requested_source parameter specifies the name of the source being requested.
// The type parameter specifies the kind of inclusion request being made.
// The requesting_source parameter specifies the name of the source containing
// the #include request.  The includer owns the result object and its contents,
// and both must remain valid until the release callback is called on the result
// object.
shaderc_include_result* resolve_include(void* user_data, const char* requested_source, int type, const char* requesting_source, size_t include_depth)
{

	char *include_path = calloc(1, 512);
	int include_path_length = snprintf(include_path, 128, "%sshaders\\%s", source_dir, requested_source);
	struct Blob file = file_read_entire_file(include_path);

	printf("Including %s\n", include_path);
	shader_includes_str[shader_includes_length] = include_path;
	shader_includes_str_length[shader_includes_length] = include_path_length;
	shader_includes_length += 1;

	shaderc_include_result *result = calloc(sizeof(shaderc_include_result), 1);
	result->source_name = include_path;
	result->source_name_length = include_path_length;
	result->content = file.data;
	result->content_length = file.size;
	return result;
}


// An includer callback type for destroying an include result.
void free_include(void* user_data, shaderc_include_result* include_result)
{
}

struct MaterialJson
{
	const char* vertex_shader;
	const char* pixel_shader;
	uint32_t vertex_shader_length;
	uint32_t pixel_shader_length;
	uint32_t render_pass_id;
};

struct MaterialJson parse_material_json(const char* source_path)
{
	struct MaterialJson material = {0};

	struct Blob material_json_file = file_read_entire_file(source_path);
	struct json_value_s* root = json_parse_ex(material_json_file.data, material_json_file.size, COOKER_JSON_PARSE_FLAGS , NULL, NULL, NULL);
	assert(root->type == json_type_object);
	struct json_object_s* object = (struct json_object_s*)root->payload;
	for (struct json_object_element_s* it = object->start; it != NULL; it = it->next) {
		if (strcmp(it->name->string, "vertex_shader") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			material.vertex_shader = value->string;
			material.vertex_shader_length = value->string_size;
		}
		if (strcmp(it->name->string, "pixel_shader") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			material.pixel_shader = value->string;
			material.pixel_shader_length = value->string_size;
		}
		if (strcmp(it->name->string, "render_pass") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			for (uint32_t ipass = 0; ipass < RENDER_PASSES_COUNT; ++ipass) {
				if (strcmp(RENDER_PASSES[ipass].name, value->string) == 0) {
					material.render_pass_id = ipass;
					break;
				}
			}
			assert(material.render_pass_id < RENDER_PASSES_COUNT);
		}
	}

	return material;
}

struct SkeletalMeshJson
{
	const char* source_file;
	const char* anim_skeleton;
	const char* mesh;
	uint32_t source_file_length;
	uint32_t anim_skeleton_length;
	uint32_t mesh_length;
};

struct SkeletalMeshJson parse_skeletal_mesh_json(const char* source_path)
{
	struct SkeletalMeshJson skeletal_mesh = {0};

	struct Blob skeletal_mesh_json_file = file_read_entire_file(source_path);
	struct json_value_s* root = json_parse_ex(skeletal_mesh_json_file.data, skeletal_mesh_json_file.size, COOKER_JSON_PARSE_FLAGS , NULL, NULL, NULL);
	assert(root->type == json_type_object);
	struct json_object_s* object = (struct json_object_s*)root->payload;
	for (struct json_object_element_s* it = object->start; it != NULL; it = it->next) {
		if (strcmp(it->name->string, "source_file") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			skeletal_mesh.source_file = value->string;
			skeletal_mesh.source_file_length = value->string_size;
		}
		if (strcmp(it->name->string, "anim_skeleton") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			skeletal_mesh.anim_skeleton = value->string;
			skeletal_mesh.anim_skeleton_length = value->string_size;
		}
		if (strcmp(it->name->string, "mesh") == 0) {
			struct json_string_s *value = json_value_as_string(it->value);
			assert(value != NULL);
			skeletal_mesh.mesh = value->string;
			skeletal_mesh.mesh_length = value->string_size;
		}
	}

	return skeletal_mesh;
}

int cook_material()
{
	char source_path[512];
	char dep_path[512];
	snprintf(source_path, sizeof(source_path), "%s%s", source_dir, relative_path);
	snprintf(dep_path, sizeof(dep_path), "%s%s.dep", cooking_dir, relative_path);

	// Parse JSON
	char vertex_shader_path[512] = {0};
	char pixel_shader_path[512] = {0};
	struct MaterialJson json = parse_material_json(source_path);
	snprintf(vertex_shader_path, sizeof(vertex_shader_path), "%s%.*s", source_dir, json.vertex_shader_length, json.vertex_shader);
	snprintf(pixel_shader_path, sizeof(pixel_shader_path), "%s%.*s", source_dir, json.pixel_shader_length, json.pixel_shader);

	// Compile shaders
	struct Blob vshader_file = file_read_entire_file(vertex_shader_path);
	struct Blob pshader_file = file_read_entire_file(pixel_shader_path);

	shaderc_compiler_t shader_compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
	shaderc_compile_options_set_generate_debug_info(options);
	shaderc_compile_options_set_include_callbacks(options, resolve_include, free_include, NULL);

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
	material.id = string_to_id(relative_path, strlen(relative_path));
	material.vertex_shader_bytecode.size = shaderc_result_get_length(vshader_result);
	material.vertex_shader_bytecode.data = (char*)shaderc_result_get_bytes(vshader_result);
	material.pixel_shader_bytecode.size = shaderc_result_get_length(pshader_result);
	material.pixel_shader_bytecode.data = (char*)shaderc_result_get_bytes(pshader_result);
	material.render_pass_id = json.render_pass_id;

	uint32_t serializer_capacity = (4 << 10) + material.vertex_shader_bytecode.size + material.pixel_shader_bytecode.size;
	Serializer serializer = serialize_begin_write_file(serializer_capacity);
	Serialize_MaterialAsset(&serializer, &material);
	char dest_path[512] = {0};
	snprintf(dest_path, sizeof(dest_path), "%s%u", cooking_dir, material.id);
	serialize_end_write_file(&serializer, dest_path);

	// Save dep file to disk
	char dep_content[512] = {0};
	int32_t dep_cursor = 0;

	for (uint32_t iinclude = 0; iinclude < shader_includes_length; ++iinclude) {
		dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", shader_includes_str[iinclude]);
	}

	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", vertex_shader_path);
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", pixel_shader_path);


	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "OUTPUT: %s\n", dest_path);
	file_write_entire_file(dep_path, (struct Blob){dep_content, dep_cursor});

	return 0;
}

int cook_compute_program()
{
	char source_path[512];
	char dep_path[512];
	snprintf(source_path, sizeof(source_path), "%s%s", source_dir, relative_path);
	snprintf(dep_path, sizeof(dep_path), "%s%s.dep", cooking_dir, relative_path);

	// Compile shaders
	struct Blob shader_file = file_read_entire_file(source_path);

	shaderc_compiler_t shader_compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
	shaderc_compile_options_set_generate_debug_info(options);
	shaderc_compile_options_set_include_callbacks(options, resolve_include, free_include, NULL);
	shaderc_compilation_result_t shader_result = shaderc_compile_into_spv(shader_compiler,
								       shader_file.data, shader_file.size,
								       shaderc_compute_shader, source_path, "main", options);
	if (shaderc_result_get_compilation_status(shader_result) != shaderc_compilation_status_success) {
		size_t num_warnings = shaderc_result_get_num_warnings(shader_result);
		size_t num_errors = shaderc_result_get_num_errors(shader_result);
		const char* error_msg = shaderc_result_get_error_message(shader_result);
		printf("%zu warnings. %zu errors.\n%s\n", num_warnings, num_errors, error_msg);
		return 1;
	}

	// Save material to disk
	struct ComputeProgramAsset compute_program = {0};
	compute_program.id = string_to_id(relative_path, strlen(relative_path));
	compute_program.shader_bytecode.size = shaderc_result_get_length(shader_result);
	compute_program.shader_bytecode.data = (char*)shaderc_result_get_bytes(shader_result);

	uint32_t serializer_capacity = (4 << 10) + compute_program.shader_bytecode.size;
	Serializer serializer = serialize_begin_write_file(serializer_capacity);
	Serialize_ComputeProgramAsset(&serializer, &compute_program);
	char dest_path[512] = {0};
	snprintf(dest_path, sizeof(dest_path), "%s%u", cooking_dir, compute_program.id);
	serialize_end_write_file(&serializer, dest_path);

	// Save dep file to disk
	char dep_content[512] = {0};
	int32_t dep_cursor = 0;
	for (uint32_t iinclude = 0; iinclude < shader_includes_length; ++iinclude) {
		dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", shader_includes_str[iinclude]);
	}
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", source_path);
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

	// parse json
	char fbx_file_path[512];
	struct SkeletalMeshJson json = parse_skeletal_mesh_json(source_path);
	snprintf(fbx_file_path, sizeof(fbx_file_path), "%s%.*s", source_dir, (int)json.source_file_length, json.source_file);
	// parse FBX
	ufbx_load_opts opts = { 0 }; // Optional, pass NULL for defaults
        opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
        opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Z;
        opts.target_axes.front = UFBX_COORDINATE_AXIS_NEGATIVE_Y;

        opts.target_unit_meters = 1.0f;
	ufbx_error error; // Optional, pass NULL if you don't care about errors
	ufbx_scene *scene = ufbx_load_file(fbx_file_path, &opts, &error);
	if (!scene) {
		fprintf(stderr, "Failed to load: %s\n", error.description.data);
		return 1;
	}

	// Import skeleton from the nodes hierarchy
	ufbx_node *anim_skeleton_node = NULL;
	ufbx_node *mesh_node = NULL;

	for (size_t i = 0; i < scene->nodes.count; i++) {
		ufbx_string name = scene->nodes.data[i]->name;
		if (name.length == json.anim_skeleton_length && strncmp(name.data, json.anim_skeleton, name.length) == 0) {
			assert(anim_skeleton_node == NULL);
			anim_skeleton_node = scene->nodes.data[i];
		}
		if (name.length == json.mesh_length && strncmp(name.data, json.mesh, name.length) == 0) {
			assert(mesh_node == NULL);
			mesh_node = scene->nodes.data[i];
			assert(mesh_node->mesh != NULL);
		}
	}
	assert(anim_skeleton_node != NULL);
	assert(mesh_node != NULL);
	fprintf(stderr, "Found anim skeleton: %s\n", anim_skeleton_node->name.data);
	fprintf(stderr, "Found mesh: %s\n", mesh_node->name.data);
	uint32_t skeleton_id = ufbx_string_to_id(anim_skeleton_node->name);
	uint32_t skeletal_mesh_id = ufbx_string_to_id(mesh_node->name);
	// get root bone
	ufbx_node *root_bone = NULL;
	for (size_t i = 0; i < anim_skeleton_node->children.count; i++) {
		if (anim_skeleton_node->children.data[i]->bone != NULL) {
			root_bone = anim_skeleton_node->children.data[i];
			break;
		}
	}
	assert(root_bone != NULL);
	// get bind pose
	ufbx_pose *bind_pose = root_bone->bind_pose;
	assert(bind_pose != NULL);
	// get all bones
	ufbx_node **bones = (ufbx_node**)calloc(MAX_BONES_PER_MESH, sizeof(ufbx_node*));
	bones[0] = root_bone;
	uint32_t bones_length = 1;
	for (size_t i = root_bone->typed_id + 1; i < scene->nodes.count; i++) {
		ufbx_node *node = scene->nodes.data[i];
		if (node->node_depth <= root_bone->node_depth) {
			break;
		}

		if (node->bone) {
			assert(bones_length < MAX_BONES_PER_MESH);
			bones[bones_length++] = node;
			fprintf(stderr, "Importing bone %s\n", node->name.data);
		}
	}
	fprintf(stderr, "Imported %u bones\n", bones_length);
	fprintf(stderr, "Root bone: %s\n", root_bone->name.data);
	// get bone parent indices
	uint8_t *bones_parent = calloc(MAX_BONES_PER_MESH, sizeof(uint8_t));
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
	struct Animation *animations = calloc(MAX_ANIMATIONS_PER_ASSET, sizeof(struct Animation));
	assert(scene->anim_stacks.count < MAX_ANIMATIONS_PER_ASSET);
	for (size_t istack = 0; istack < scene->anim_stacks.count; istack++) {
		ufbx_anim_stack *stack = scene->anim_stacks.data[istack];
		ufbx_anim *anim = stack->anim;
		ufbx_bake_opts opts = {0};
		opts.key_reduction_enabled = false;
		ufbx_baked_anim *bake = ufbx_bake_anim(scene, anim, &opts, NULL);
		assert(bake);
		animations[istack].id = ufbx_string_to_id(stack->name);
		animations[istack].skeleton_id = skeleton_id;
		fprintf(stderr, "Importing anim %u: %s (%zu frames)\n", animations[istack].id, stack->name.data, bake->nodes.count);
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

			// extract the root motion track into a separate track
			struct AnimTrack *out_track = &animations[istack].tracks[ibone];
			if (node == root_bone) {
				assert(ibone == 0);
				out_track = &animations[istack].root_motion_track;

				// set the regular track to identity
				struct AnimTrack *out_regular_track = &animations[istack].tracks[ibone];
				out_regular_track->translations.length = baked_node->translation_keys.count;
				out_regular_track->translations.data = calloc(baked_node->translation_keys.count, sizeof(Float3));
				out_regular_track->rotations.length = baked_node->rotation_keys.count;
				out_regular_track->rotations.data = calloc(baked_node->rotation_keys.count, sizeof(Quat));
				out_regular_track->scales.length = baked_node->scale_keys.count;
				out_regular_track->scales.data = calloc(baked_node->scale_keys.count, sizeof(Float3));

				for (uint32_t itrans = 0; itrans < baked_node->translation_keys.count; ++itrans) {
					out_regular_track->translations.data[itrans] = (Float3){0};
				}
				for (uint32_t irot = 0; irot < baked_node->rotation_keys.count; ++irot) {
					out_regular_track->rotations.data[irot] = (Quat){0.0f, 0.0f, 0.0f, 1.0f};
				}
				for (uint32_t iscale = 0; iscale < baked_node->scale_keys.count; ++iscale) {
					out_regular_track->scales.data[iscale] = 1.0f;
				}
			}

			out_track->translations.length = baked_node->translation_keys.count;
			out_track->translations.data = calloc(baked_node->translation_keys.count, sizeof(Float3));
			struct Float3List translations = out_track->translations;
			ufbx_baked_vec3_list baked_translations = baked_node->translation_keys;
			for (uint32_t itrans = 0; itrans < baked_node->translation_keys.count; ++itrans) {
				translations.data[itrans].x = baked_translations.data[itrans].value.x;
				translations.data[itrans].y = baked_translations.data[itrans].value.y;
				translations.data[itrans].z = baked_translations.data[itrans].value.z;
			}
			out_track->rotations.length = baked_node->rotation_keys.count;
			out_track->rotations.data = calloc(baked_node->rotation_keys.count, sizeof(Quat));
			struct QuatList rotations = out_track->rotations;
			ufbx_baked_quat_list baked_rotations = baked_node->rotation_keys;
			for (uint32_t itrans = 0; itrans < baked_node->rotation_keys.count; ++itrans) {
				rotations.data[itrans].x = baked_rotations.data[itrans].value.x;
				rotations.data[itrans].y = baked_rotations.data[itrans].value.y;
				rotations.data[itrans].z = baked_rotations.data[itrans].value.z;
				rotations.data[itrans].w = baked_rotations.data[itrans].value.w;
			}
			out_track->scales.length = baked_node->scale_keys.count;
			out_track->scales.data = calloc(baked_node->scale_keys.count, sizeof(Float3));
			struct FloatList scales = out_track->scales;
			ufbx_baked_vec3_list baked_scales = baked_node->scale_keys;
			for (uint32_t iscale = 0; iscale < baked_node->scale_keys.count; ++iscale) {
				bool is_uniform = (fabs(baked_scales.data[iscale].value.x - baked_scales.data[iscale].value.y) < 0.2f);
				is_uniform &= (fabs(baked_scales.data[iscale].value.y - baked_scales.data[iscale].value.z) < 0.2f);
				float sum = baked_scales.data[iscale].value.x + baked_scales.data[iscale].value.y + baked_scales.data[iscale].value.z;
				float average = sum / 3.0f;
				if (!is_uniform) {
					fprintf(stderr, "non uniform scale found in anim %s bone %s\n", stack->name.data, node->name.data);
					assert(!"non uniform scale!");
				}

				scales.data[iscale] = average;
			}

			if (node == root_bone) {
				Float3 origin_translation = translations.data[0];
				for (uint32_t itrans = 0; itrans < baked_node->translation_keys.count; ++itrans) {
					translations.data[itrans] = float3_sub(translations.data[itrans], origin_translation);
				}
			}
		}
	}

	// Import the character mesh
	assert(mesh_node->mesh->skin_deformers.count == 1);
	ufbx_skin_deformer *skin = mesh_node->mesh->skin_deformers.data[0];
	for (uint32_t icluster = 0; icluster < skin->clusters.count; ++icluster) {
		ufbx_skin_cluster *cluster = skin->clusters.data[icluster];
		fprintf(stderr, "Found cluster %s\n", cluster->bone_node->name.data);
	}
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
	uint32_t mesh_indices_length = mesh_node->mesh->num_faces * 3;
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
	for (uint32_t iface = 0; iface < mesh_node->mesh->num_faces; ++iface) {
		ufbx_face face = mesh_node->mesh->faces.data[iface];
		assert(face.num_indices == 3);
		for (uint32_t corner = 0; corner < face.num_indices; ++corner) {
			uint32_t index = face.index_begin + corner;
			ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh_node->mesh->vertex_position, index);
			ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh_node->mesh->vertex_normal, index);
			ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh_node->mesh->vertex_uv, index);
			// ufbx_vec3 tangent = ufbx_get_vertex_vec3(&mesh_node->mesh->vertex_tangent, index);
			// ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh_node->mesh->vertex_color, index);
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
			uint32_t vertex = mesh_node->mesh->vertex_indices.data[index];
			ufbx_skin_vertex skin_vertex = skin->vertices.data[vertex];
			assert(skin_vertex.num_weights <= MAX_WEIGHTS);
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
	skeletal_mesh_with_animations.anim_skeleton.id = skeleton_id;
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
	skeletal_mesh_with_animations.skeletal_mesh.id = skeletal_mesh_id;
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

	uint32_t serializer_capacity = (16 << 20);
	Serializer serializer = serialize_begin_write_file(serializer_capacity);
	Serialize_SkeletalMeshWithAnimationsAsset(&serializer, &skeletal_mesh_with_animations);
	char dest_path[512];
	snprintf(dest_path, sizeof(dest_path), "%s%u", cooking_dir, skeletal_mesh_with_animations.skeletal_mesh.id);
	serialize_end_write_file(&serializer, dest_path);
	// Save dep file to disk
	char dep_content[512] = {0};
	int32_t dep_cursor = 0;
	dep_cursor += snprintf(dep_content + dep_cursor, (512 - dep_cursor), "INPUT: %s\n", fbx_file_path);
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
	} else if (strcmp(asset_type, "compute") == 0) {
		return cook_compute_program();
	} else if (strcmp(asset_type, "skeletalmesh") == 0) {
		return cook_fbx();
	}

	return 1;
}

#include "asset.c"
#include "anim.c"
#include <ufbx.c>
