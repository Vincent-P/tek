#include "tek.h"
#include <json.h>

#if !defined(XXH_INLINE_ALL)
#define XXH_INLINE_ALL
#include <xxhash.h>
#endif

const char* tek_CancelType_str[TEK_CANCEL_TYPE_COUNT] = {
	"single",
	"list",
};

const char* tek_CancelCondition_str[TEK_CANCEL_CONDITION_COUNT] = {
	"true",
	"eoa",
};

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

static struct tek_Cancel tek_read_cancel(struct json_object_s *cancel_obj)
{
	struct tek_Cancel cancel = {0};
	
	if (cancel_obj == NULL) {
		return cancel;
	}
	
	for (struct json_object_element_s* it = cancel_obj->start; it != NULL; it = it->next) {
		if (strcmp(it->name->string, "type") == 0) {
			struct json_string_s *type = json_value_as_string(it->value);
			for (tek_CancelType itype = 0; itype < TEK_CANCEL_TYPE_COUNT; ++itype) {
				if (strcmp(type->string, tek_CancelType_str[itype]) == 0) {
					cancel.type = itype;
				}
			}
		}
		if (strcmp(it->name->string, "condition") == 0) {
			struct json_string_s *condition = json_value_as_string(it->value);
			for (tek_CancelCondition icondition = 0; icondition < TEK_CANCEL_CONDITION_COUNT; ++icondition) {
				if (strcmp(condition->string, tek_CancelCondition_str[icondition]) == 0) {
					cancel.condition = icondition;
				}
			}
		}
		json_object_get_string_id(it, "to", &cancel.to_move_id);
		json_object_get_u8(it, "motion_input", &cancel.motion_input);
		if (json_object_get_u8(it, "action_input", &cancel.action_input)) {
			cancel.action_input = (1 << cancel.action_input);
		}
	}
	
	return cancel;
}

static void tek_read_json_move_cancels(struct tek_Move *move, struct json_array_s *cancels_array)
{
	if (cancels_array == NULL) {
		return;
	}
	
	uint32_t cancel_index = 0;
	for (struct json_array_element_s* it = cancels_array->start; it != NULL && cancel_index < MAX_CANCELS_PER_MOVE; it = it->next) {
		struct json_object_s *cancel_obj = json_value_as_object(it->value);
		if (cancel_obj == NULL) {
			continue;
		}
		
		move->cancels[cancel_index] = tek_read_cancel(cancel_obj);
		cancel_index++;
	}
}

static void tek_read_json_move(struct tek_Character *character, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}
	
	struct tek_Move move = {0};
	struct json_array_s *cancels = NULL;
	
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		json_object_get_string_id(it, "name", &move.id);
		json_object_get_string_id(it, "animation", &move.animation_id);
		json_object_get_u8(it, "startup", &move.startup);
		json_object_get_u8(it, "active", &move.active);
		json_object_get_u8(it, "recovery", &move.recovery);
		json_object_get_u8(it, "hitbox", &move.hitbox);
		json_object_get_u8(it, "base_damage", &move.base_damage);
		json_object_get_u8(it, "hitstun", &move.hitstun);
		json_object_get_u8(it, "blockstun", &move.blockstun);
		
		if (strcmp(it->name->string, "cancels") == 0) {
			cancels = json_value_as_array(it->value);
		}
	}
	
	// Process cancels if they exist
	tek_read_json_move_cancels(&move, cancels);
	
	uint32_t imove = character->moves_length;
	assert(imove < ARRAY_LENGTH(character->moves));
	character->moves[imove] = move;
	character->moves_length += 1;
}

static void tek_read_json_cancel_group(struct tek_Character *character, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}

	uint32_t group_index = character->cancel_groups_length;
	assert(group_index < MAX_CANCEL_GROUPS);
	character->cancel_groups_length += 1;
	struct tek_CancelGroup *group = &character->cancel_groups[group_index];
	
	struct json_array_s *cancels = NULL;
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		json_object_get_string_id(it, "name", &group->id);
		
		if (strcmp(it->name->string, "cancels") == 0) {
			cancels = json_value_as_array(it->value);
		}
	}
	
	// Process cancels in the group
	if (cancels != NULL) {
		uint32_t cancel_index = 0;
		for (struct json_array_element_s* it = cancels->start; it != NULL && cancel_index < MAX_CANCELS_PER_GROUP; it = it->next) {
			struct json_object_s *cancel_obj = json_value_as_object(it->value);
			if (cancel_obj == NULL) {
				continue;
			}
			
			group->cancels[cancel_index] = tek_read_cancel(cancel_obj);
			cancel_index++;
		}
		group->cancels_length = cancel_index;
	}
	
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

static void tek_read_json_hitbox(struct tek_Character *character, struct json_object_s *obj)
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
	
	uint32_t ibox = character->hitboxes_length;
	assert(ibox < ARRAY_LENGTH(character->hitboxes_radius));
	character->hitboxes_radius[ibox] = radius;
	character->hitboxes_height[ibox] = height;
	character->hitboxes_bone_id[ibox] = bone_id;
	character->hitboxes_length += 1;
}

void tek_read_character_json()
{
	const char* source_path = "assets/michel.character.json";
	
	struct tek_Character character = {0};
	
	struct Blob character_json_file = file_read_entire_file(source_path);
	struct json_value_s* root = json_parse_ex(character_json_file.data, character_json_file.size, json_parse_flags_allow_c_style_comments | json_parse_flags_allow_trailing_comma , NULL, NULL, NULL);
	assert(root->type == json_type_object);

	struct json_array_s *moves = NULL;
	struct json_object_s *group_cancels = NULL;
	struct json_array_s *hurtboxes = NULL;
	struct json_array_s *hitboxes = NULL;
	
	struct json_object_s* object = (struct json_object_s*)root->payload;
	for (struct json_object_element_s* it = object->start; it != NULL; it = it->next) {

		json_object_get_string_id(it, "name", &character.id);
		json_object_get_string_id(it, "skeletal_mesh", &character.skeletal_mesh_id);
		json_object_get_string_id(it, "anim_skeleton", &character.anim_skeleton_id);
		
		if (strcmp(it->name->string, "moves") == 0) {
			moves = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "group_cancels") == 0) {
			group_cancels = json_value_as_object(it->value);
		}
		if (strcmp(it->name->string, "hurtboxes") == 0) {
			hurtboxes = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "hitboxes") == 0) {
			hitboxes = json_value_as_array(it->value);
		}
	}

	if (moves != NULL) {
		for (struct json_array_element_s* it = moves->start; it != NULL; it = it->next) {
			struct json_object_s *move = json_value_as_object(it->value);
			tek_read_json_move(&character, move);
		}
	}
	if (group_cancels != NULL) {
		tek_read_json_cancel_group(&character, group_cancels);
	}
	if (hurtboxes != NULL) {
		for (struct json_array_element_s* it = hurtboxes->start; it != NULL; it = it->next) {
			struct json_object_s *hurtbox = json_value_as_object(it->value);
			tek_read_json_hurtbox(&character, hurtbox);
		}
	}
	if (hitboxes != NULL) {
		for (struct json_array_element_s* it = hitboxes->start; it != NULL; it = it->next) {
			struct json_object_s *hitbox = json_value_as_object(it->value);
			tek_read_json_hitbox(&character, hitbox);
		}
	}

	tek_characters[0] = character;
}
