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


#define SV_ADD_IMPL(revision, type, field) if (serializer->version>=revision) Serialize_##type(serializer, &value->field);
#define SV_REM_IMPL(added, removed, type, field, default) type field=default; if (serializer->version>=added && serializer->version<removed) Serialize_##type(serializer, field);
// Use 2 macros to force macro expansion when calling SV_ADD
#define SV_ADD(revision, type, field) SV_ADD_IMPL(revision, type, field)
#define SV_REM(added, removed, type, field, default) SV_REM(added, removed, type, field, default)
#define SerializeSimpleType(T) \
void Serialize_##T(Serializer *serializer, T *value) \
{ \
	assert(serializer->cursor + sizeof(T) <= serializer->buffer.size); \
	T *buffer_as_##T = (T*)(serializer->buffer.data + serializer->cursor); \
	serializer->cursor += sizeof(T); \
	\
	if (serializer->is_reading) { \
		*value = *buffer_as_##T; \
	} else { \
		*buffer_as_##T = *value; \
	} \
}

void SerializeBytes(Serializer *serializer, void *data, uint32_t size)
{
	assert(serializer->cursor + size <= serializer->buffer.size);
	void *buffer = serializer->buffer.data + serializer->cursor;
	serializer->cursor += size;
		
	if (serializer->is_reading) {
		memcpy(data, buffer, size);
	} else {
		memcpy(buffer, data, size);
	}
}

SerializeSimpleType(uint8_t)
SerializeSimpleType(uint32_t)
SerializeSimpleType(uint64_t)
SerializeSimpleType(float)

void Serialize_Blob(Serializer *serializer, struct Blob *value)
{
	SV_ADD(SV_INITIAL, uint32_t, size);

	if (serializer->is_reading) {
		value->data = calloc(1, value->size);
	}
	SerializeBytes(serializer, value->data, value->size);
}
