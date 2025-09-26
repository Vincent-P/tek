#pragma once


struct Atlas2D;
struct atlas2d_Allocation
{
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

uint32_t atlas2d_get_size(uint32_t size, uint32_t min_alloc_size);
void atlas2d_init(struct Atlas2D *atlas, uint32_t size, uint32_t min_alloc_size);
void atlas2d_clear(struct Atlas2D *atlas);
// return true if success
bool atlas2d_allocate(struct Atlas2D *atlas, uint32_t size, struct atlas2d_Allocation *alloc);
// no deallocate. need to clear and allocate again.
