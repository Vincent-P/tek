#pragma once

// AABBGGRR
#define DD_RED   (uint32_t)0xff0000ff
#define DD_GREEN (uint32_t)0xff00ff00
#define DD_BLUE  (uint32_t)0xffff8800
#define DD_WHITE (uint32_t)0xffffffff
#define DD_BLACK (uint32_t)0xff000000

#define DD_HALF_ALPHA    (uint32_t)0x80ffffff
#define DD_QUARTER_ALPHA (uint32_t)0x40ffffff

void debug_draw_reset(void);
void debug_draw_point(Float3 p);
void debug_draw_line(Float3 from, Float3 to, uint32_t color);
