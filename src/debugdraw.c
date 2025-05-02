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

void debug_draw_cylinder(Float3 center, float radius, float height, uint32_t color)
{
	uint32_t CIRCLE_SEGMENTS = 8;
	for (uint32_t i = 0; i < CIRCLE_SEGMENTS; ++i) {
		float angle = 2 * 3.14 * (float)i / (float)CIRCLE_SEGMENTS;
		float next_angle = 2 * 3.14 * (float)(i+1) / (float)CIRCLE_SEGMENTS;

		Float3 point_top;
		point_top.x = center.x + cosf(angle) * radius;
		point_top.y = center.y + sinf(angle) * radius;
		point_top.z = center.z + height * 0.5f;
		Float3 point_bottom = point_top;
		point_bottom.z = center.z - height * 0.5f;
		
		Float3 next_point_top;
		next_point_top.x = center.x + cosf(next_angle) * radius;
		next_point_top.y = center.y + sinf(next_angle) * radius;
		next_point_top.z = center.z + height * 0.5f;
		Float3 next_point_bottom = next_point_top;
		next_point_bottom.z = center.z - height * 0.5f;
		
		debug_draw_line(point_top, next_point_top, color);
		debug_draw_line(point_top, point_bottom, color);
		debug_draw_line(point_bottom, next_point_bottom, color);
	}
}
