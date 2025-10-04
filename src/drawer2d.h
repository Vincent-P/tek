#pragma once
#define DRAWER_2D_VERTEX_CAPACITY (8 * 1024)
#define DRAWER_2D_INDEX_CAPACITY (32 * 1024)

struct Vertex2D
{
	float x;
	float y;
	float u;
	float v;
	uint32_t color;
};

struct Drawer2D
{
	uint32_t current_color;
	float viewport_width;
	float viewport_height;

	uint32_t current_vertices_length;	
	uint32_t current_indices_length;

	struct Vertex2D current_vertices[DRAWER_2D_VERTEX_CAPACITY];
	uint32_t current_indices[DRAWER_2D_INDEX_CAPACITY];

	uint32_t glyph_cache_texture;
	struct GlyphCache *glyph_cache;
};

void drawer2d_init(struct Drawer2D *drawer, struct Renderer *renderer);
void drawer2d_reset_frame(struct Drawer2D *drawer);
void drawer2d_draw_rect(struct Drawer2D *drawer, float top, float left, float width, float height, uint32_t color);

struct DrawerTextInfo
{
	float size_px;
	uint32_t color;
};
void drawer2d_text_bounds(struct Drawer2D *drawer, const char* text, uint32_t text_length, struct DrawerTextInfo options, float *out_bounds_x, float *out_bounds_y);
void drawer2d_draw_text(struct Drawer2D *drawer, const char* text, uint32_t text_length, float top, float left, float width, float height, struct DrawerTextInfo options);
