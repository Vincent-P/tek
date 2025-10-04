#include "atlas2d.h"

struct atlas2d_Tile
{
	uint16_t x;
	uint16_t y;
	uint16_t w; // "real" allocation size
	uint16_t h; // "real" allocation size
	uint8_t level;
	uint8_t min_level;
};

struct Atlas2D
{
	uint32_t size;
	uint32_t min_alloc_size;
	uint32_t levels_count;
	uint32_t tiles_count;
	struct atlas2d_Tile *tiles;
};

uint32_t atlas2d_tzcnt(uint32_t n)
{
	unsigned long first_found_bit = 0;
	unsigned char not_zero = _BitScanForward(&first_found_bit, n);
	return n == 0 ? 0 : first_found_bit;
}

/**
Hierarchical indexing:

                              | 0 |
      | 1 |           | 2 |            | 3 |               | 4 |
| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 
 **/
uint32_t atlas2d_quadtree_index(uint32_t level, uint32_t tile)
{

	uint32_t index = 0;
	// skip previous levels
	for (uint32_t ilevel = 0; ilevel < level; ++ilevel){
		index += 1 << (ilevel*2);
	}
	// index our own tile
	index += tile;
	return index;
}

uint32_t atlas2d_quadtree_index_child(uint32_t level, uint32_t tile, uint32_t child_corner)
{
	uint32_t index = 0;
	// skip previous levels (the child is 1 level further)
	for (uint32_t ilevel = 0; ilevel < level+1; ++ilevel){
		index += 1 << (ilevel*2);
	}
	// index each tile from level has 4 children
	index += tile * 4;
	// index corner
	index += child_corner;
	return index;
}

uint32_t atlas2d_get_size(uint32_t size, uint32_t min_alloc_size)
{
	assert(is_pow2(size));
	assert(is_pow2(min_alloc_size));
	uint32_t level_count = atlas2d_tzcnt(size) - atlas2d_tzcnt(min_alloc_size) + 1;
	uint32_t total_tiles_count = 0;
	for (uint32_t ilevel = 0; ilevel <= level_count; ++ilevel) {
		uint32_t tiles_count_per_level = 1 << (ilevel * 2);
		total_tiles_count += tiles_count_per_level;
	}
	return sizeof(struct Atlas2D) + sizeof(struct atlas2d_Tile) * total_tiles_count;
}

void atlas2d_init(struct Atlas2D *atlas, uint32_t size, uint32_t min_alloc_size)
{
	assert(is_pow2(size));
	assert(is_pow2(min_alloc_size));
	atlas->size = size;
	atlas->min_alloc_size = min_alloc_size;
	atlas->tiles = (struct atlas2d_Tile*)(atlas+1);
	atlas->levels_count = atlas2d_tzcnt(size) - atlas2d_tzcnt(min_alloc_size) + 1;
	uint32_t total_tiles_count = 0;
	for (uint32_t ilevel = 0; ilevel <= atlas->levels_count; ++ilevel) {
		uint32_t tiles_count_per_level = 1 << (ilevel * 2);
		total_tiles_count += tiles_count_per_level;
	}
	atlas->tiles_count = total_tiles_count;

	// Initialize tiles
	uint32_t level_count = atlas2d_tzcnt(size) - atlas2d_tzcnt(min_alloc_size);
	for (uint32_t ilevel = 0; ilevel <= level_count; ++ilevel) {
		uint32_t tiles_count_per_level = 1 << (ilevel * 2);
		uint32_t tiles_count_per_level_row = 1 << (ilevel);
		
		for (uint32_t itile = 0; itile < tiles_count_per_level; ++itile) {
			uint32_t tile_index = atlas2d_quadtree_index(ilevel, itile);
			assert(tile_index < atlas->tiles_count);
			
			uint32_t x = itile % tiles_count_per_level_row;
			uint32_t y = itile / tiles_count_per_level_row;

			atlas->tiles[tile_index] = (struct atlas2d_Tile){0};
			atlas->tiles[tile_index].x = x * (size >> ilevel);
			atlas->tiles[tile_index].y = y * (size >> ilevel);
			atlas->tiles[tile_index].w = (size >> ilevel);
			atlas->tiles[tile_index].h = (size >> ilevel);
			atlas->tiles[tile_index].level = ilevel;
			atlas->tiles[tile_index].min_level = 0;
		}
		
	}
}

void atlas2d_clear(struct Atlas2D *atlas)
{
	for (uint32_t itile = 0; itile < atlas->tiles_count; ++itile)  {
		atlas->tiles[itile].min_level = 0;
	}
}

uint32_t atlas2d_find_tile(struct Atlas2D *atlas, uint32_t parent_level, uint32_t parent_itile, uint32_t level)
{
	uint32_t parent_tile_index = atlas2d_quadtree_index(parent_level, parent_itile);
	assert(parent_tile_index < atlas->tiles_count);
			
	for (uint32_t icorner = 0; icorner < 4; ++icorner) {
		uint32_t child_level = parent_level + 1;
		uint32_t child_itile = parent_itile*4+icorner;
		uint32_t child_tile_index = atlas2d_quadtree_index_child(parent_level, parent_itile, icorner);

		assert(child_tile_index < atlas->tiles_count);
		struct atlas2d_Tile child_tile = atlas->tiles[child_tile_index];
		
		// child is already allocated
		if (level < child_tile.min_level) {
			continue;
		}

		// valid tile found
		if (level == child_tile.level) {
			atlas->tiles[parent_tile_index].min_level = level;
			atlas->tiles[child_tile_index].min_level = atlas->levels_count;
			return child_tile_index;
		}

		// if we have children, search there
		if (child_level + 1 < atlas->levels_count) {
			uint32_t child_found_tile = atlas2d_find_tile(atlas, child_level, child_itile, level);
			if (child_found_tile < atlas->tiles_count) {
				// the child found a valid tile
				return child_found_tile;
			}
		}
	}

	return ~0u;
}

bool atlas2d_allocate(struct Atlas2D *atlas, uint32_t size, struct atlas2d_Allocation *alloc)
{
	uint32_t size_pow2 = next_pow2(size);

	uint32_t desired_level = atlas2d_tzcnt(atlas->size) - atlas2d_tzcnt(size_pow2);
	if (desired_level >= atlas->levels_count) {
		desired_level = atlas->levels_count - 1;
	}

	uint32_t found_tile_index = atlas2d_find_tile(atlas, 0, 0, desired_level);
	if (found_tile_index >= atlas->tiles_count) {
		return false;
	}

	atlas->tiles[found_tile_index].w = size;
	atlas->tiles[found_tile_index].h = size;

	alloc->x = atlas->tiles[found_tile_index].x;
	alloc->y = atlas->tiles[found_tile_index].y;
	alloc->w = atlas->tiles[found_tile_index].w;
	alloc->h = atlas->tiles[found_tile_index].h;
	
	return true;
}
