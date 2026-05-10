static char ui_mem[1024];
static Arena ui_arena;




void ui_new_frame()
{
	ui_arena.beg = ui_mem;
	ui_arena.end = ui_mem + sizeof(ui_mem);
}

static void ui_handle_button_interaction(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
	(void)elementId;
	bool *pressed = (bool *)userData;
	// Pointer state allows you to detect mouse down / hold / release
	bool button_clicked = pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME || pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME;;
	if (pressed != NULL && button_clicked) {
		*pressed = true;
	}
}

void ui_button(const char *label, bool *clicked)
{
	Clay_String label_string = (struct Clay_String) {.isStaticallyAllocated = false, .length = (int32_t)strlen(label), .chars = label};

	Clay_TextElementConfig *label_text_config = CLAY_TEXT_CONFIG({ .fontSize = 32, .textColor = {255, 255, 255, 255}, .textAlignment = CLAY_TEXT_ALIGN_RIGHT });

	CLAY({.id = CLAY_SID(label_string), .layout = { .padding = CLAY_PADDING_ALL(8) } }) {
		Clay_OnHover(ui_handle_button_interaction, (uintptr_t)clicked);
		CLAY_TEXT(label_string, label_text_config);
	}
}

const char* ui_string(const char* string, uint32_t length)
{
	char *copy = arena_new(&ui_arena, char, length);
	memcpy(copy, string, length);
	return copy;
}


// basic key type helpers
uint64_t ui_key_null(void) { return 0llu; }

uint64_t ui_key_from_int(uint64_t n)
{
	n = (~n) + (n << 21); // n = (n << 21) - n - 1;
	n = n ^ (n >> 24);
	n = (n + (n << 3)) + (n << 8); // n * 265
	n = n ^ (n >> 14);
	n = (n + (n << 2)) + (n << 4); // n * 21
	n = n ^ (n >> 28);
	n = n + (n << 31);
	return n;
}

uint64_t ui_key_combine(uint64_t a, uint64_t b)
{
	return a ^ ( b + 0x9e3779b9 + (a<<6) + (a>>2));
}

// construct a widget, looking up from the cache if
// possible, and pushing it as a new child of the
// active parent.
UiWidgetId ui_widget_make(UiHierarchy *h, UiWidgetFlags flags, const char *string)
{
	// Generate a new key
	uint64_t key = 0;
	for (uint32_t i = 0; i < h->id_stack_length; ++i) {
		key = ui_key_combine(key, h->id_stack[i]);
	}
	key = ui_key_combine(key, ui_key_from_int(h->global_generation));
	h->global_generation += 1;

	// Find or create element in the cache
	UiWidgetId index = key % ARRAY_LENGTH(h->widgets);
	if (h->widgets[index].key == 0) {
		// definitely not here. insert.
	}
	else if (h->widgets[index].key == key) {
		// found
	} else {
		UiWidgetId next = h->widgets[index].hash_next;
		for (; next != 0; next = h->widgets[next].hash_next) {
			if (h->widgets[next].key == key) {
				// found break;
				index = next;
			}
		}
		if (next == 0) {
			// not found: find empty slot starting from (expected slot + 1) and link it to hash list
			uint32_t i = 0;
			for (; i < ARRAY_LENGTH(h->widgets); ++i) {
				UiWidgetId new_index = (UiWidgetId)((index + 1 + i) % ARRAY_LENGTH(h->widgets));
				if (h->widgets[new_index].key == 0) {

					UiWidgetId prev_next = h->widgets[index].hash_next;
					// make head of the list points to the new node
					h->widgets[index].hash_next = new_index;
					// make the previous next of the head points to the new node
					if (prev_next) {
						ASSERT(h->widgets[prev_next].prev == index);
						h->widgets[prev_next].prev = new_index;
					}

					// link the new node to both the head and previous next
					h->widgets[new_index].key = key;
					h->widgets[new_index].hash_prev = index;
					h->widgets[new_index].hash_next = prev_next;

					index = new_index;
					break;
				}
			}
			ASSERT(i < ARRAY_LENGTH(h->widgets)); // Check that we found a slot
		}
	}

	// If the slot was empty before we need to set persistent data
	if (h->widgets[index].key == 0) {
		h->widgets[index].key = key;
	}
	// widget is at index
	h->widgets[index].flags = flags;
	h->widgets[index].string = string;
	h->widgets[index].first_child = 0;
	h->widgets[index].last_child = 0;
	h->widgets[index].next = 0;
	h->widgets[index].prev = 0;
	h->widgets[index].parent = 0;
	h->widgets[index].last_frame_touched_index = h->current_frame;

	// link to tree
	if (h->parent_stack_length != 0) {
		// Set parent
		UiWidgetId p = h->parent_stack[h->parent_stack_length-1];
		h->widgets[index].parent = p;
		// Update parent's last and first child
		if (h->widgets[p].first_child == 0) {
			h->widgets[p].first_child = index;
		}
		h->widgets[p].last_child = index;
		// Link to parent's children
		UiWidgetId prev_sibling = h->widgets[p].last_child;
		h->widgets[index].prev = prev_sibling;
		h->widgets[index].next = 0;
		if (prev_sibling) {
			h->widgets[prev_sibling].next = index;
		}
	}

	return index;
}


UiWidgetId ui_widget_make_fmt(UiHierarchy *h, UiWidgetFlags flags, const char *fmt, ...);

// some other possible building parameterizations
void ui_widget_equip_display_string(UiHierarchy *h, UiWidgetId widget, const char *string);
// void ui_widget_equip_child_layout_axis(UiWidgetIdwidget, Axis2 axis);

// managing the parent stack
UiWidgetId ui_push_parent(UiHierarchy *h, UiWidgetId widget)
{
	uint32_t stack_len = h->parent_stack_length;
	ASSERT(stack_len + 1 < ARRAY_LENGTH(h->parent_stack));
	h->parent_stack[stack_len] = widget;
	h->parent_stack_length += 1;
	return widget;
}

UiWidgetId ui_pop_parent(UiHierarchy *h)
{
	ASSERT(h->parent_stack_length != 0);
	h->parent_stack_length -= 1;
	return h->parent_stack[h->parent_stack_length + 1];
}
