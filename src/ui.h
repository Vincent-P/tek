#pragma once

void ui_new_frame();
void ui_button(const char *label, bool *clicked);
const char* ui_string(const char* string, uint32_t length);


typedef enum UiSizeKind UiSizeKind;
enum UiSizeKind
{
	UI_SIZE_KIND_NULL,
	UI_SIZE_KIND_PIXELS,
	UI_SIZE_KIND_TEXT,
	UI_SIZE_KIND_PERCENT,
	UI_SIZE_KIND_CHILDREN_SUM,
	UI_SIZE_KIND_FLEX,
};

typedef struct UiSize UiSize;
struct UiSize
{
	UiSizeKind kind;
	float value;
	float strictness; // how much a size can stretch down, 0 = can be reduced to 0px, 1 = must maintain 100%
};

typedef uint32_t UiWidgetFlags;
enum
{
  UI_WidgetFlag_Clickable       = (1<<0),
  UI_WidgetFlag_ViewScroll      = (1<<1),
  UI_WidgetFlag_DrawText        = (1<<2),
  UI_WidgetFlag_DrawBorder      = (1<<3),
  UI_WidgetFlag_DrawBackground  = (1<<4),
  UI_WidgetFlag_DrawDropShadow  = (1<<5),
  UI_WidgetFlag_Clip            = (1<<6),
  UI_WidgetFlag_HotAnimation    = (1<<7),
  UI_WidgetFlag_ActiveAnimation = (1<<8),
  // ...
};

typedef uint16_t UiWidgetId;

typedef struct UiWidget UiWidget;
struct UiWidget
{
	// tree links
	UiWidgetId parent;
	UiWidgetId first_child;
	UiWidgetId last_child;
	UiWidgetId next;
	UiWidgetId prev;

	// key+generation info
	uint64_t key;
	uint64_t last_frame_touched_index;
	UiWidgetId hash_prev;
	UiWidgetId hash_next;

	// per frame info
	UiWidgetFlags flags;
	UiSize semantic_size[2];
	int layout_axis;
	const char* string;

	// computed every frame
	float computed_rel_position[2];
	float computed_size[2];

	// persistent data
	float hot_transition;
	float active_transition;
};

typedef struct UiHierarchy UiHierarchy;
struct UiHierarchy
{
	UiWidget widgets[65536];
	UiWidgetId parent_stack[128];
	uint64_t id_stack[128];
	uint32_t widgets_length;
	uint32_t parent_stack_length;
	uint32_t id_stack_length;
	uint64_t global_generation;

	uint32_t current_frame;
};


uint64_t ui_key_null(void);
uint64_t ui_key_from_int(uint64_t n);
uint64_t ui_key_combine(uint64_t a, uint64_t b);

void ui_layout_end_frame(UiHierarchy *h, UiWidgetId root);
void ui_imgui(UiHierarchy *h, UiWidgetId root);

// set widget
UiWidgetId ui_widget_make(UiHierarchy *h, UiWidgetFlags flags, const char *string);
void ui_widget_set_display_string(UiHierarchy *h, UiWidgetId widget, const char *string);
void ui_widget_set_child_layout_axis(UiHierarchy *h, UiWidgetId widget, int axis); // X = 0, Y =1
void ui_widget_set_size_x(UiHierarchy *h, UiWidgetId widget, UiSize size);
void ui_widget_set_size_y(UiHierarchy *h, UiWidgetId widget, UiSize size);


// managing the parent stack
UiWidgetId ui_push_parent(UiHierarchy *h, UiWidgetId widget);
UiWidgetId ui_pop_parent(UiHierarchy *h);
