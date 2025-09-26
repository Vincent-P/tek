#pragma once
#include "atlas2d.h"

#define DRAWER_2D_VERTEX_CAPACITY (8 * 1024)
#define DRAWER_2D_INDEX_CAPACITY (32 * 1024)

struct Vertex2D
{
	float x;
	float y;
	uint32_t color;
};

struct Drawer2D
{
	uint32_t current_color;
	float viewport_width;
	float viewport_height;

	uint32_t current_vertices_length;	
	uint32_t current_indices_length;

	struct Atlas2D *atlas;
	
	struct Vertex2D current_vertices[DRAWER_2D_VERTEX_CAPACITY];
	uint32_t current_indices[DRAWER_2D_INDEX_CAPACITY];
};

void drawer2d_init(struct Drawer2D *drawer);
void drawer2d_reset_frame(struct Drawer2D *drawer);
void drawer2d_draw_rect(struct Drawer2D *drawer, float top, float left, float width, float height, uint32_t color);
