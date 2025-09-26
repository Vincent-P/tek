#include "drawer2d.h"

void drawer2d_init(struct Drawer2D *drawer)
{
	uint32_t atlas_size = atlas2d_get_size(1024, 256);
	drawer->atlas = (struct Atlas2D*)calloc(1, atlas_size);

	atlas2d_init(drawer->atlas, 1024, 256);
}

void drawer2d_reset_frame(struct Drawer2D *drawer)
{
	drawer->current_vertices_length = 0;
	drawer->current_indices_length = 0;
}

void drawer2d_draw_rect(struct Drawer2D *drawer, float top, float left, float width, float height, uint32_t color)
{
	assert(drawer->current_vertices_length + 4 < DRAWER_2D_VERTEX_CAPACITY);
	assert(drawer->current_indices_length + 6 < DRAWER_2D_INDEX_CAPACITY);

	uint32_t v = drawer->current_vertices_length;
	uint32_t i = drawer->current_indices_length;

	drawer->current_vertices[v + 0] = (struct Vertex2D){.x = left,         .y = top,          .color = color};
	drawer->current_vertices[v + 1] = (struct Vertex2D){.x = left + width, .y = top,          .color = color};
	drawer->current_vertices[v + 2] = (struct Vertex2D){.x = left,         .y = top + height, .color = color};
	drawer->current_vertices[v + 3] = (struct Vertex2D){.x = left + width, .y = top + height, .color = color};

	drawer->current_indices[i + 0] = v + 0;
	drawer->current_indices[i + 1] = v + 2;
	drawer->current_indices[i + 2] = v + 1;
	drawer->current_indices[i + 3] = v + 2;
	drawer->current_indices[i + 4] = v + 3;
	drawer->current_indices[i + 5] = v + 1;

	drawer->current_vertices_length += 4;
	drawer->current_indices_length += 6;
}
