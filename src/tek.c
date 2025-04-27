#include "tek.h"
#include <json.h>

#if !defined(XXH_INLINE_ALL)
#define XXH_INLINE_ALL
#include <xxhash.h>
#endif

uint32_t string_to_id(const char *s, size_t l)
{
	XXH64_hash_t hash = XXH64(s, l, 0);
	return (uint32_t)hash;
}

bool json_object_get_string_id(struct json_object_element_s *it, const char *field_name, uint32_t *id)
{
	if (strcmp(it->name->string, field_name) == 0) {
		struct json_string_s *s = json_value_as_string(it->value);
		if (s != NULL) {
			*id = string_to_id(s->string, s->string_size);
			fprintf(stderr, "[json] read string %s = %s (%u)\n", field_name, s->string, *id);
			return true;
		}
	}
	return false;
}

bool json_object_get_u8(struct json_object_element_s *it, const char *field_name, uint8_t *u8)
{
	if (strcmp(it->name->string, field_name) == 0) {
		struct json_number_s *n = json_value_as_number(it->value);
		if (n != NULL) {
			int value = atol(n->number);
			*u8 = (uint8_t)value;
			fprintf(stderr, "[json] read u8 %s = %u\n", field_name, (uint32_t)*u8);
			return true;
		}
	}
	return false;
}

bool json_object_get_float(struct json_object_element_s *it, const char *field_name, float *f)
{
	if (strcmp(it->name->string, field_name) == 0) {
		struct json_number_s *n = json_value_as_number(it->value);
		if (n != NULL) {
			*f = strtof(n->number, NULL);
			fprintf(stderr, "[json] read float %s = %f\n", field_name, (double)*f);
			return true;
		}
	}
	return false;
}

static void tek_read_json_stance_move(struct tek_Stance *stance, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}
	
	struct tek_Move move = {0};
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		json_object_get_string_id(it, "name", &move.id);
		json_object_get_string_id(it, "animation", &move.animation_id);
		json_object_get_u8(it, "startup", &move.startup);
		json_object_get_u8(it, "active", &move.active);
		json_object_get_u8(it, "recovery", &move.recovery);
		json_object_get_u8(it, "hitstun", &move.hitstun);
		json_object_get_u8(it, "blockstun", &move.blockstun);
		json_object_get_u8(it, "base_damage", &move.base_damage);
		json_object_get_u8(it, "motion_input", &move.motion_input);
		if (json_object_get_u8(it, "action_input", &move.action_input)) {
			move.action_input = (1 << move.action_input);
		}
		json_object_get_u8(it, "transition_input", &move.transition_input);
		json_object_get_string_id(it, "transition_stance", &move.transition_stance_id);
	}
	uint32_t imove = stance->moves_length;
	assert(imove < ARRAY_LENGTH(stance->moves));
	stance->moves[imove] = move;
	stance->moves_length += 1;
}

static void tek_read_json_stance(struct tek_Character *character, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}
	
	struct tek_Stance stance = {0};
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		json_object_get_string_id(it, "name", &stance.id);
		if (strcmp(it->name->string, "moves") == 0) {
			struct json_array_s *moves = json_value_as_array(it->value);
			if (moves != NULL) {
				for (struct json_array_element_s* it = moves->start; it != NULL; it = it->next) {
					struct json_object_s *move = json_value_as_object(it->value);
					tek_read_json_stance_move(&stance, move);
				}
			}
		}
	}
	
	uint32_t istance = character->stances_length;
	assert(istance < ARRAY_LENGTH(character->stances));
	character->stances[istance] = stance;
	character->stances_length += 1;
}

static void tek_read_json_hurtbox(struct tek_Character *character, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}
	float radius = 0.0f;
	float height = 0.0f;
	uint32_t bone_id = 0;
	
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		json_object_get_float(it, "radius", &radius);
		json_object_get_float(it, "height", &height);
		json_object_get_string_id(it, "bone", &bone_id);
	}
	
	uint32_t ibox = character->hurtboxes_length;
	assert(ibox < ARRAY_LENGTH(character->hurtboxes_radius));
	character->hurtboxes_radius[ibox] = radius;
	character->hurtboxes_height[ibox] = height;
	character->hurtboxes_bone_id[ibox] = bone_id;
	character->hurtboxes_length += 1;
}

void tek_read_character_json()
{
	const char* source_path = "assets/michel.character.json";
	
	struct tek_Character character = {0};
	
	struct Blob character_json_file = file_read_entire_file(source_path);
	struct json_value_s* root = json_parse_ex(character_json_file.data, character_json_file.size, json_parse_flags_allow_c_style_comments | json_parse_flags_allow_trailing_comma , NULL, NULL, NULL);
	assert(root->type == json_type_object);

	struct json_array_s *stances = NULL;
	struct json_array_s *hurtboxes = NULL;
	struct json_array_s *hitboxes = NULL;
	
	struct json_object_s* object = (struct json_object_s*)root->payload;
	for (struct json_object_element_s* it = object->start; it != NULL; it = it->next) {

		json_object_get_string_id(it, "name", &character.id);
		json_object_get_string_id(it, "skeletal_mesh", &character.skeletal_mesh_id);
		json_object_get_string_id(it, "anim_skeleton", &character.anim_skeleton_id);
		
		if (strcmp(it->name->string, "stances") == 0) {
			stances = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "hurtboxes") == 0) {
			hurtboxes = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "hitboxes") == 0) {
			hitboxes = json_value_as_array(it->value);
		}
	}

	if (stances != NULL) {
		for (struct json_array_element_s* it = stances->start; it != NULL; it = it->next) {
			struct json_object_s *stance = json_value_as_object(it->value);
			tek_read_json_stance(&character, stance);
		}
	}
	if (hurtboxes != NULL) {
		for (struct json_array_element_s* it = hurtboxes->start; it != NULL; it = it->next) {
			struct json_object_s *hurtbox = json_value_as_object(it->value);
			tek_read_json_hurtbox(&character, hurtbox);
		}
	}

	tek_characters[0] = character;
}
