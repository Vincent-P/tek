#include "debugdraw.h"

#define DD_POINTS_CAPACITY 128
#define DD_LINES_CAPACITY 1024

struct DebugDraw
{
	Float3 points[DD_POINTS_CAPACITY];
	uint32_t points_length;
	Float3 lines_from[DD_LINES_CAPACITY];
	Float3 lines_to[DD_LINES_CAPACITY];
	uint32_t lines_col[DD_LINES_CAPACITY];
	uint32_t lines_length;
};
static struct DebugDraw g_dd;

void debug_draw_reset()
{
	g_dd.points_length = 0;
	g_dd.lines_length = 0;
}

void debug_draw_point(Float3 p)
{
	if (g_dd.points_length + 1 < ARRAY_LENGTH(g_dd.points)) {
		
		g_dd.points[g_dd.points_length++] = p;
	
	}
}

void debug_draw_line(Float3 from, Float3 to, uint32_t color)
{
	if (g_dd.lines_length + 1 < ARRAY_LENGTH(g_dd.lines_from)) {
		
		g_dd.lines_from[g_dd.lines_length] = from;
		g_dd.lines_to[g_dd.lines_length] = to;
		g_dd.lines_col[g_dd.lines_length] = color;
		g_dd.lines_length += 1;
	
	}
}
