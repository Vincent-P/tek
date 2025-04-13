#pragma once

typedef struct Serializer Serializer;
struct Blob
{
	void *data;
	uint32_t size;
};

struct Serializer
{
	struct Blob buffer;
	uint32_t cursor;
	uint32_t version;
	bool is_reading;
};

enum SerializerVersions
{
	SV_INITIAL=0, // first commit
	SV_LATEST=SV_INITIAL,
};


#define SV_ADD(revision, type, field) if (serializer->version>=revision) Serialize_##type(serializer, &value->field);
#define SV_REM(added, removed, type, field, default) type field=default; if (serializer->version>=added && serializer->version<removed) Serialize_##type(serializer, field);

void Serialize_uint32_t(Serializer *serializer, uint32_t *value)
{
	assert(serializer->cursor + sizeof(uint32_t) <= serializer->buffer.size);
	uint32_t *buffer_as_uint32 = (uint32_t*)(serializer->buffer.data + serializer->cursor);
	serializer->cursor += sizeof(uint32_t);
		
	if (serializer->is_reading) {
		*value = *buffer_as_uint32;
	} else {
		*buffer_as_uint32 = *value;
	}
}

void Serialize_Blob(Serializer *serializer, struct Blob *value)
{
	SV_ADD(SV_INITIAL, uint32_t, size);

	assert(serializer->cursor + value->size <= serializer->buffer.size);
	uint32_t *buffer = (uint32_t*)(serializer->buffer.data + serializer->cursor);
	serializer->cursor += value->size;
		
	if (serializer->is_reading) {
		value->data = calloc(1, value->size);
		memcpy(value->data, buffer, value->size);
	} else {
		memcpy(buffer, value->data, value->size);
	}
}
