#include "tek.h"
#include <json.h>

#if !defined(XXH_INLINE_ALL)
#define XXH_INLINE_ALL
#include <xxhash.h>
#endif

const char* tek_CancelType_str[TEK_CANCEL_TYPE_COUNT] = {
	"single",
	"loop",
	"continue",
	"list",
};

const char* tek_CancelCondition_str[TEK_CANCEL_CONDITION_COUNT] = {
	"true",
	"eoa",
};

const char* tek_HitLevel_str[TEK_HIT_LEVEL_COUNT] = {
	"",
	"high",
	"mid",
	"low",
	"unblockable",
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

static struct tek_HitCondition tek_read_hit_condition(struct json_object_s *hit_condition_obj)
{
	struct tek_HitCondition hit_condition = {0};
	if (hit_condition_obj == NULL) {
		return hit_condition;
	}
	
	for (struct json_object_element_s* it = hit_condition_obj->start; it != NULL; it = it->next) {
		json_object_get_string_id(it, "reactions", &hit_condition.reactions_id);
		json_object_get_u8(it, "damage", &hit_condition.damage);
	}
	
	return hit_condition;
}

static void tek_read_json_move_hit_conditions(struct tek_Move *move, struct json_array_s *hit_conditions_array)
{
	if (hit_conditions_array == NULL) {
		return;
	}
	
	uint32_t hit_condition_index = 0;
	for (struct json_array_element_s* it = hit_conditions_array->start; it != NULL && hit_condition_index < MAX_HIT_CONDITIONS_PER_MOVE; it = it->next) {
		struct json_object_s *hit_condition_obj = json_value_as_object(it->value);
		if (hit_condition_obj == NULL) {
			continue;
		}
		
		move->hit_conditions[hit_condition_index] = tek_read_hit_condition(hit_condition_obj);
		hit_condition_index++;
	}
	move->hit_conditions_length = hit_condition_index;
	assert(move->hit_conditions_length <= ARRAY_LENGTH(move->hit_conditions));
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
		json_object_get_u8(it, "frame", &cancel.starting_frame);
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

	move->cancels_length = cancel_index;
	assert(move->cancels_length <= ARRAY_LENGTH(move->cancels));
}

static void tek_read_json_move(struct tek_Character *character, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}
	
	struct tek_Move move = {0};
	struct json_array_s *cancels = NULL;
	struct json_array_s *hit_conditions = NULL;
	struct json_string_s *movename = NULL;
	
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		
		if (json_object_get_string_id(it, "name", &move.id)) {
			movename = json_value_as_string(it->value);
		}
		json_object_get_string_id(it, "animation", &move.animation_id);
		
		// hit
		if (strcmp(it->name->string, "hit_level") == 0) {
			struct json_string_s *hit_level = json_value_as_string(it->value);
			for (enum tek_HitLevel ihit_level = 0; ihit_level < TEK_HIT_LEVEL_COUNT; ++ihit_level) {
				if (strcmp(hit_level->string, tek_HitLevel_str[ihit_level]) == 0) {
					move.hit_level = ihit_level;
				}
			}
		}
		json_object_get_u8(it, "hitbox", &move.hitbox);
		
		// frame data
		json_object_get_u8(it, "startup", &move.startup);
		json_object_get_u8(it, "active", &move.active);
		json_object_get_u8(it, "recovery", &move.recovery);
		
		if (strcmp(it->name->string, "hit_conditions") == 0) {
			hit_conditions = json_value_as_array(it->value);
		}
		
		if (strcmp(it->name->string, "cancels") == 0) {
			cancels = json_value_as_array(it->value);
		}
	}
	
	// Process cancels if they exist
	tek_read_json_move_cancels(&move, cancels);
	
	tek_read_json_move_hit_conditions(&move, hit_conditions);
	
	uint32_t imove = character->moves_length;
	assert(imove < ARRAY_LENGTH(character->moves));
	character->moves[imove] = move;
	strncpy(character->move_names[imove].string,
		movename->string,
		sizeof(character->move_names[imove].string)-1);
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
		assert(group->cancels_length <= ARRAY_LENGTH(group->cancels));
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

static void tek_read_json_hit_reactions(struct tek_Character *character, struct json_object_s *obj)
{
	if (obj == NULL) {
		return;
	}

	struct tek_HitReactions reactions = {0};	
	for (struct json_object_element_s* it = obj->start; it != NULL; it = it->next) {
		json_object_get_string_id(it, "name", &reactions.id);
		json_object_get_string_id(it, "standing_move", &reactions.standing_move);
		json_object_get_string_id(it, "standing_counter_hit_move", &reactions.standing_counter_hit_move);
		json_object_get_string_id(it, "standing_block_move", &reactions.standing_block_move);
	}
	
	uint32_t i = character->hit_reactions_length;
	assert(i < ARRAY_LENGTH(character->hit_reactions));
	character->hit_reactions[i] = reactions;
	character->hit_reactions_length++;
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
	character.max_health = 100;
	
	struct Blob character_json_file = file_read_entire_file(source_path);
	struct json_value_s* root = json_parse_ex(character_json_file.data, character_json_file.size, json_parse_flags_allow_c_style_comments | json_parse_flags_allow_trailing_comma , NULL, NULL, NULL);
	assert(root->type == json_type_object);

	struct json_array_s *moves = NULL;
	struct json_array_s *group_cancels = NULL;
	struct json_array_s *hit_reactions = NULL;
	struct json_array_s *hurtboxes = NULL;
	struct json_array_s *hitboxes = NULL;
	
	struct json_object_s* object = (struct json_object_s*)root->payload;
	for (struct json_object_element_s* it = object->start; it != NULL; it = it->next) {

		json_object_get_string_id(it, "name", &character.id);
		json_object_get_float(it, "radius", &character.colision_radius);
		json_object_get_string_id(it, "skeletal_mesh", &character.skeletal_mesh_id);
		json_object_get_string_id(it, "anim_skeleton", &character.anim_skeleton_id);
		
		if (strcmp(it->name->string, "moves") == 0) {
			moves = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "group_cancels") == 0) {
			group_cancels = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "hurtboxes") == 0) {
			hurtboxes = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "hitboxes") == 0) {
			hitboxes = json_value_as_array(it->value);
		}
		if (strcmp(it->name->string, "hit_reactions") == 0) {
			hit_reactions = json_value_as_array(it->value);
		}
	}

	if (moves != NULL) {
		for (struct json_array_element_s* it = moves->start; it != NULL; it = it->next) {
			struct json_object_s *move = json_value_as_object(it->value);
			tek_read_json_move(&character, move);
		}
	}
	if (group_cancels != NULL) {
		for (struct json_array_element_s* it = group_cancels->start; it != NULL; it = it->next) {
			struct json_object_s *group_cancel = json_value_as_object(it->value);
			tek_read_json_cancel_group(&character, group_cancel);
		}
	}
	if (hit_reactions != NULL) {
		for (struct json_array_element_s* it = hit_reactions->start; it != NULL; it = it->next) {
			struct json_object_s *hit_reaction = json_value_as_object(it->value);
			tek_read_json_hit_reactions(&character, hit_reaction);
		}
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

struct tek_Move *tek_character_find_move(struct tek_Character *character, uint32_t id)
{
	assert(id != 0);
	for (uint32_t imove = 0; imove < ARRAY_LENGTH(character->moves); ++imove) {
		if (character->moves[imove].id == id) {
			return character->moves + imove;
		}
	}
	return NULL;
}

struct tek_CancelGroup *tek_character_find_cancel_group(struct tek_Character *character, uint32_t id)
{
	assert(id != 0);
	for (uint32_t icancel_group = 0; icancel_group < ARRAY_LENGTH(character->cancel_groups); ++icancel_group) {
		if (character->cancel_groups[icancel_group].id == id) {
			return character->cancel_groups + icancel_group;
		}
	}
	return NULL;
}

struct tek_HitReactions *tek_character_find_hit_reaction(struct tek_Character *character, uint32_t id)
{
	assert(id != 0);
	for (uint32_t ihit_reaction = 0; ihit_reaction < ARRAY_LENGTH(character->hit_reactions); ++ihit_reaction) {
		if (character->hit_reactions[ihit_reaction].id == id) {
			return character->hit_reactions + ihit_reaction;
		}
	}
	return NULL;
}
