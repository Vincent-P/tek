#pragma once
#include "ui.h"

enum UiHelperConstants
{
	UI_AXIS_X = 0,
	UI_AXIS_Y = 1,
};

#define UI_SIZE_FLEX (UiSize){UI_SIZE_KIND_FLEX, 1.0f}
#define UI_SIZE_PERCENT(x) (UiSize){UI_SIZE_KIND_PERCENT, x}
#define UI_SIZE_PIXELS(x) (UiSize){UI_SIZE_KIND_PIXELS, x}

UiWidgetId ui_push_column(UiHierarchy *h, const char* string, UiWidgetFlags flags);
void ui_pop_column(UiHierarchy *h);

UiWidgetId ui_label(UiHierarchy *h, const char *id, const char* string, uint32_t string_length, float font_size, uint32_t color);

// widget_make + push_parent + set_size
UiWidgetId ui_push_container(UiHierarchy *h, const char *string, UiWidgetFlags flags, UiSize x, UiSize y);
