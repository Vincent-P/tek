#include "drawer2d.h"
#include "atlas2d.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <utf8.h>


#define GLYPH_CACHE_RASTERIZED_GLYPH_CAPACITY 1024
#define GLYPH_CACHE_ATLAS_SIZE 1024

struct RasterizedGlyph
{
	int32_t codepoint;
	float size;
	float u0;
	float u1;
	float v0;
	float v1;
	float x;
	float y;
	float w;
	float h;
	float advance_w;
};

struct GlyphCache
{
	unsigned char ttf_buffer[1 << 20];

	struct Atlas2D *atlas;
	uint32_t atlas_texture;
	struct Renderer *renderer; // for texture upload
	stbtt_fontinfo main_font;
	struct RasterizedGlyph rasterized_glyphs[GLYPH_CACHE_RASTERIZED_GLYPH_CAPACITY];
	uint32_t rasterized_glyphs_length;
};

void drawer2d_init(struct Drawer2D *drawer, struct Renderer *renderer)
{
	drawer->glyph_cache = (struct GlyphCache*)calloc(1, sizeof(struct GlyphCache));
	drawer->glyph_cache->renderer = renderer;

	uint32_t atlas_min_size = 32;
	uint32_t atlas_size = atlas2d_get_size(GLYPH_CACHE_ATLAS_SIZE, atlas_min_size);
	drawer->glyph_cache->atlas = (struct Atlas2D*)calloc(1, atlas_size);
	atlas2d_init(drawer->glyph_cache->atlas, GLYPH_CACHE_ATLAS_SIZE, atlas_min_size);

	fread(drawer->glyph_cache->ttf_buffer, 1, 1<<20, fopen("c:/windows/fonts/CascadiaMono.ttf", "rb"));
	stbtt_InitFont(&drawer->glyph_cache->main_font, drawer->glyph_cache->ttf_buffer, stbtt_GetFontOffsetForIndex(drawer->glyph_cache->ttf_buffer, 0));

	drawer->glyph_cache->atlas_texture = 10;
	void *empty = calloc(GLYPH_CACHE_ATLAS_SIZE, GLYPH_CACHE_ATLAS_SIZE);
	memset(empty, 255, GLYPH_CACHE_ATLAS_SIZE*GLYPH_CACHE_ATLAS_SIZE);
	new_texture(renderer->device, drawer->glyph_cache->atlas_texture, GLYPH_CACHE_ATLAS_SIZE, GLYPH_CACHE_ATLAS_SIZE, PG_FORMAT_R8_UNORM, empty, GLYPH_CACHE_ATLAS_SIZE*GLYPH_CACHE_ATLAS_SIZE);


	drawer->glyph_cache_texture = drawer->glyph_cache->atlas_texture;

	struct atlas2d_Allocation empty_atlas_alloc = {0};
	bool atlas_success = atlas2d_allocate(drawer->glyph_cache->atlas, 1, &empty_atlas_alloc);
	assert(atlas_success);
}

void drawer2d_reset_frame(struct Drawer2D *drawer)
{
	drawer->current_vertices_length = 0;
	drawer->current_indices_length = 0;

	drawer2d_set_clip_rect(drawer, 0.0f, 0.0f, drawer->viewport_width, drawer->viewport_height);
}

void drawer2d_set_clip_rect(struct Drawer2D *drawer, float x, float y, float w, float h)
{
    drawer->clip_x = x;
    drawer->clip_y = y;
    drawer->clip_w = w;
    drawer->clip_h = h;
}

static void _drawer2d_draw_rect(struct Drawer2D *drawer, float top, float left, float width, float height, uint32_t color, uint32_t texture, float u0, float u1, float v0, float v1)
{
	assert(drawer->current_vertices_length + 4 < DRAWER_2D_VERTEX_CAPACITY);
	assert(drawer->current_indices_length + 6 < DRAWER_2D_INDEX_CAPACITY);

	uint32_t v = drawer->current_vertices_length;
	uint32_t i = drawer->current_indices_length;

	float clip_left = drawer->clip_x;
	float clip_top = drawer->clip_y;
	float clip_right = drawer->clip_x + drawer->clip_w;
	float clip_bottom = drawer->clip_y + drawer->clip_h;

	drawer->current_vertices[v + 0] = (struct Vertex2D){.x = left,         .y = top,          .u = u0, .v = v0, clip_left, clip_top, clip_right, clip_bottom, .color = color};
	drawer->current_vertices[v + 1] = (struct Vertex2D){.x = left + width, .y = top,          .u = u1, .v = v0, clip_left, clip_top, clip_right, clip_bottom, .color = color};
	drawer->current_vertices[v + 2] = (struct Vertex2D){.x = left,         .y = top + height, .u = u0, .v = v1, clip_left, clip_top, clip_right, clip_bottom, .color = color};
	drawer->current_vertices[v + 3] = (struct Vertex2D){.x = left + width, .y = top + height, .u = u1, .v = v1, clip_left, clip_top, clip_right, clip_bottom, .color = color};

	drawer->current_indices[i + 0] = v + 0;
	drawer->current_indices[i + 1] = v + 2;
	drawer->current_indices[i + 2] = v + 1;
	drawer->current_indices[i + 3] = v + 2;
	drawer->current_indices[i + 4] = v + 3;
	drawer->current_indices[i + 5] = v + 1;

	drawer->current_vertices_length += 4;
	drawer->current_indices_length += 6;
}

void drawer2d_draw_rect(struct Drawer2D *drawer, float top, float left, float width, float height, uint32_t color)
{
	_drawer2d_draw_rect(drawer, top, left, width, height, color, drawer->glyph_cache_texture, 0.0f, 0.0f, 0.0f, 0.0f);
}

bool glyph_cache_get_rasterized_glyph(struct GlyphCache *glyph_cache, int32_t codepoint, float size_px, struct RasterizedGlyph *out_glyph)
{
	struct Renderer *renderer = glyph_cache->renderer;
	stbtt_fontinfo *font = &glyph_cache->main_font;

	// 1. Find already rasterized glyph for codepoint C and size S.
	for (uint32_t iglyph = 0; iglyph < glyph_cache->rasterized_glyphs_length; ++iglyph) {
		struct RasterizedGlyph glyph = glyph_cache->rasterized_glyphs[iglyph];
		if (codepoint == glyph.codepoint && size_px == glyph.size) {
			*out_glyph = glyph;
			return true;
		}
	}

	// 2. Rasterize glyph into CPU bitmap
	float height_px = size_px;
	float scale = stbtt_ScaleForPixelHeight(font, height_px);

	int bitmap_width = 0;
	int bitmap_height = 0;
	int bitmap_left = 0;
	int bitmap_top = 0;
	unsigned char *bitmap = stbtt_GetCodepointBitmap(font, 0.0f, scale, codepoint, &bitmap_width, &bitmap_height, &bitmap_left, &bitmap_top);

#if 0
	for (int j = 0; j < bitmap_height; ++j) {
		for (int i = 0; i < bitmap_width; ++i)
			putchar(" .:ioVM@"[bitmap[j*bitmap_width+i]>>5]);
		putchar('\n');
	}
#endif

	int advance_width = 0;
	int left_side_bearing = 0;
	stbtt_GetCodepointHMetrics(font, codepoint, &advance_width, &left_side_bearing);


	// 4. Try to allocate temp GPU data for the CPU bitmap
	void* gpu_temp_data = renderer_temp_allocate_gpu(renderer, bitmap_width*bitmap_height);
	if (gpu_temp_data == NULL) {
		// return replacement glyph
		return false;
	}
	memcpy(gpu_temp_data, bitmap, bitmap_width*bitmap_height);

	// 3. Try to allocate space in the Atlas
	uint32_t size = bitmap_width > bitmap_height ? bitmap_width : bitmap_height;
	struct atlas2d_Allocation atlas_alloc = {0};
	bool atlas_success = atlas2d_allocate(glyph_cache->atlas, size, &atlas_alloc);
	if (atlas_success == false) {
		// return replacement glyph
		return false;
	}

	// 5. Issue GPU upload from temp GPU bitmap at offset given by Atlas
	if (bitmap_width > 0 && bitmap_height > 0){
		struct RendererTextureUpload upload = {0};
		upload.temp_data = gpu_temp_data;
		upload.texture = glyph_cache->atlas_texture;
		upload.x_offset = atlas_alloc.x;
		upload.y_offset = atlas_alloc.y;
		upload.width = bitmap_width;
		upload.height = bitmap_height;
		renderer_upload_texture(renderer, upload);
	}

	// 6. Insert the glyph in the list of already rasterized glyph
	struct RasterizedGlyph glyph = (struct RasterizedGlyph){
		.codepoint = codepoint,
		.size = size_px,
		.u0 = (float)atlas_alloc.x / (float)GLYPH_CACHE_ATLAS_SIZE,
		.u1 = ((float)atlas_alloc.x + (float)bitmap_width) / (float)GLYPH_CACHE_ATLAS_SIZE,
		.v0 = (float)atlas_alloc.y / (float)GLYPH_CACHE_ATLAS_SIZE,
		.v1 = ((float)atlas_alloc.y + (float)bitmap_height) / (float)GLYPH_CACHE_ATLAS_SIZE,
		.x = bitmap_left,
		.y = bitmap_top,
		.w = bitmap_width,
		.h = bitmap_height,
		.advance_w = advance_width * scale,
	};
	assert(glyph_cache->rasterized_glyphs_length + 1 <= GLYPH_CACHE_RASTERIZED_GLYPH_CAPACITY);
	glyph_cache->rasterized_glyphs[glyph_cache->rasterized_glyphs_length] = glyph;
	glyph_cache->rasterized_glyphs_length += 1;
	*out_glyph = glyph;

	return true;
}


void drawer2d_text_bounds(struct Drawer2D *drawer, const char* text, uint32_t text_length, struct DrawerTextInfo options, float *out_bounds_x, float *out_bounds_y)
{
	struct GlyphCache *glyph_cache = drawer->glyph_cache;

	int32_t codepoint = 0;
	uint32_t codepoint_length  = utf8nlen(text, text_length);
	void *text_codepoint_it = utf8codepoint(text, &codepoint);

	float bounds_x = 0.0f;
	struct RasterizedGlyph glyph = {0};

	for (uint32_t icodepoint = 0; icodepoint < codepoint_length; ++icodepoint) {
		bool success = glyph_cache_get_rasterized_glyph(glyph_cache, codepoint, options.size_px, &glyph);

		if (success) {
			bounds_x += glyph.advance_w;
		} else {
			bounds_x += options.size_px;
		}

		text_codepoint_it = utf8codepoint(text_codepoint_it, &codepoint);
	}

	*out_bounds_x = bounds_x;
	*out_bounds_y = options.size_px;
}

void drawer2d_draw_text(struct Drawer2D *drawer, const char* text, uint32_t text_length, float top, float left, float width, float height, struct DrawerTextInfo options)
{
	struct GlyphCache *glyph_cache = drawer->glyph_cache;

	int32_t codepoint = 0;
	uint32_t codepoint_length  = utf8nlen(text, text_length);
	void *text_codepoint_it = utf8codepoint(text, &codepoint);

	float cursor = 0.0f;
	struct RasterizedGlyph glyph = {0};

	top += options.size_px;

	for (uint32_t icodepoint = 0; icodepoint < codepoint_length; ++icodepoint) {
		bool success = glyph_cache_get_rasterized_glyph(glyph_cache, codepoint, options.size_px, &glyph);

		if (success) {
			_drawer2d_draw_rect(drawer, top + glyph.y, left + cursor + glyph.x, glyph.w, glyph.h, options.color, drawer->glyph_cache_texture, glyph.u0, glyph.u1, glyph.v0, glyph.v1);
			cursor += glyph.advance_w;
		} else {
			cursor += options.size_px;
		}

		if (cursor > width) {
			break;
		}

		text_codepoint_it = utf8codepoint(text_codepoint_it, &codepoint);
	}
}
