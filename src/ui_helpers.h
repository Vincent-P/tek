#pragma once
#include "ui.h"


UiWidgetId ui_push_column(UiHierarchy *h, const char* string, UiWidgetFlags flags);
void ui_pop_column(UiHierarchy *h);

UiWidgetId ui_label(UiHierarchy *h, const char *id, const char* string, uint32_t string_length, float font_size, uint32_t color);
