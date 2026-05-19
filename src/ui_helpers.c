#include "ui_helpers.h"

UiWidgetId ui_push_column(UiHierarchy *h, const char *string, UiWidgetFlags flags)
{
	UiWidgetId id = ui_push_parent(h, ui_widget_make(h, flags, string));
	ui_widget_set_layout(h, id, 1, 0.0f);
	return id;
}


void ui_pop_column(UiHierarchy *h)
{
	ui_pop_parent(h);
}

UiWidgetId ui_label(UiHierarchy *h, const char *id, const char *string, uint32_t string_length, float font_size, uint32_t color)
{
	UiWidgetId res = ui_widget_make(h, UI_WidgetFlag_DrawText, id);
	ui_widget_set_display_string(h, res, string, string_length, font_size);
	ui_widget_set_size_x(h, res, (UiSize){UI_SIZE_KIND_TEXT});
	ui_widget_set_size_y(h, res, (UiSize){UI_SIZE_KIND_TEXT});
	ui_widget_set_color(h, res, color);
	return res;
}
