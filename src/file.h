#pragma once
#include "serializer.h"

struct Blob file_read_entire_file(const char* path)
{
	FILE *f = fopen(path, "rb");
	assert(f != NULL);
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
	void *content = calloc(1, fsize);
	fread(content, fsize, 1, f);
	fclose(f);

	struct Blob result = {0};
	result.data = content;
	result.size = fsize;
	return result;
}

void file_write_entire_file(const char* path, struct Blob buffer)
{
	FILE *f = fopen(path, "wb");
	fwrite(buffer.data, buffer.size, 1, f);
	fclose(f);
}

Serializer serialize_read_file(const char* path)
{
	Serializer serializer = {0};
	serializer.buffer = file_read_entire_file(path);
	serializer.is_reading = true;
	Serialize_uint32_t(&serializer, &serializer.version);
	return serializer;
}

Serializer serialize_begin_write_file()
{
	Serializer serializer = {0};
	serializer.buffer.data = calloc(1, (16 << 10));
	serializer.buffer.size = (16 << 10);
	serializer.is_reading = false;
	serializer.version = SV_LATEST;
	Serialize_uint32_t(&serializer, &serializer.version);
	return serializer;	
}

void serialize_end_write_file(Serializer *serializer, const char* path)
{
	FILE *f = fopen(path, "wb");
	fwrite(serializer->buffer.data, serializer->cursor, 1, f);
	fclose(f);
}
