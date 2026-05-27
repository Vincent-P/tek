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

float _measure_text(const char *s, uint32_t string_length, float font_size, int axis, struct Drawer2D *drawer)
{
	struct DrawerTextInfo text_info = {0};
	text_info.size_px = font_size;
	text_info.color = 0;

	float bounds[2] = {0.0f, 0.0f};
	drawer2d_text_bounds(drawer, s, string_length, text_info, &bounds[0], &bounds[1]);
	return bounds[axis];
}

void _ui_layout_traversal(UiHierarchy *h, UiWidgetId node, struct Drawer2D *drawer)
{
	UiWidget *current = &h->widgets[node];

	// validate widget state
	ASSERT(current->layout_axis <= 1);

	// Update our size constraint written in `computed_size`
	for (int axis = 0; axis < 2; ++axis) {
		if (current->semantic_size[axis].kind == UI_SIZE_KIND_PIXELS) {
			// Pixels size override any constraint
			current->computed_size[axis] = current->semantic_size[axis].value;
		} else if (current->semantic_size[axis].kind == UI_SIZE_KIND_TEXT) {
			// Text size override any constraint
			current->computed_size[axis] = _measure_text(current->display_string, current->display_string_length, current->font_size, axis, drawer);
		} else if (current->semantic_size[axis].kind == UI_SIZE_KIND_PERCENT) {
			// Percent size multiplies with the maximum size constraint (assuming the parent constraints to its full size)
			current->computed_size[axis] *= current->semantic_size[axis].value;
		} else {
			// CHILDREN_SUM, FLEX
		}
	}

	// Substract padding to our size constraint
	current->computed_size[0] -= 2.0f * current->padding;
	current->computed_size[1] -= 2.0f * current->padding;

	// The size constraint need to be saved for children with percent sizes.
	float original_constraints[2] = {0};
	original_constraints[0] = current->computed_size[0];
	original_constraints[1] = current->computed_size[1];

	// First get size of every non-flex children, so that we can compute remaining space for flex children.
	for (UiWidgetId c = current->first_child; c != 0; ) {
		UiWidget *child = &h->widgets[c];
		bool is_child_flex = false;
		// Prepare constraint for non-flex children
		for (int axis = 0; axis < 2; ++axis) {
			if (child->semantic_size[axis].kind == UI_SIZE_KIND_FLEX) {
				is_child_flex = true;
			} else if (child->semantic_size[axis].kind == UI_SIZE_KIND_PERCENT) {
				child->computed_size[axis] = original_constraints[axis];
			} else {
				child->computed_size[axis] = current->computed_size[axis];
			}
		}
		if (!is_child_flex) {
			_ui_layout_traversal(h, c, drawer);
			// Substract the final size of the child to our size constraint
			current->computed_size[current->layout_axis] -= child->computed_size[current->layout_axis];
		}
		c = child->next;
	}

	// Compute sizes of flex children with remaining size
	float max_flex[2] = {0.0f, 0.0f};
	for (UiWidgetId c = current->first_child; c != 0; ) {
		UiWidget *child = &h->widgets[c];
		for (int axis = 0; axis < 2; ++axis) {
			if (child->semantic_size[axis].kind == UI_SIZE_KIND_FLEX) {
				max_flex[axis] += child->semantic_size[axis].value;
			}
		}
		c = child->next;
	}
	for (UiWidgetId c = current->first_child; c != 0; ) {

		UiWidget *child = &h->widgets[c];

		bool is_child_flex = false;
		// Compute flex factor per axis
		float flex_factors[2] = {0.0f, 0.0f};
		for (int axis = 0; axis < 2; ++axis) {
			if (child->semantic_size[axis].kind == UI_SIZE_KIND_FLEX) {
				is_child_flex = true;
				flex_factors[axis] = child->semantic_size[axis].value / max_flex[axis];
			} else {
				flex_factors[axis] = 1.0f;
			}
		}

		if (is_child_flex) {
			// Set size constrainst on children from the parent
			child->computed_size[0] = flex_factors[0] * current->computed_size[0];
			child->computed_size[1] = flex_factors[1] * current->computed_size[1];

			_ui_layout_traversal(h, c, drawer);

			// When a child has been laid out, update constraints
			// We don't update size constraints if the axis is flex, because the constraint contains the remaining space.
			if (child->semantic_size[current->layout_axis].kind != UI_SIZE_KIND_FLEX) {
				current->computed_size[current->layout_axis] -= child->computed_size[current->layout_axis];
			}
		}
		c = child->next;
	}

	// Now all children have computed their size, position them
	float cursor[2] = {current->padding, current->padding};
	for (UiWidgetId c = current->first_child; c != 0; ) {
		UiWidget *child = &h->widgets[c];
		for (int axis = 0; axis < 2; ++axis) {
			child->computed_rel_position[axis] = cursor[axis];
			if (axis == current->layout_axis) {
				// If the axis is the layout axis, children are positioned in order
				// so we move the cursor after each children
				cursor[axis] += child->computed_size[axis];
			}
		}
		c = child->next;
	}

	// finally compute our own size
	for (int axis = 0; axis < 2; ++axis) {
		if (current->semantic_size[axis].kind == UI_SIZE_KIND_CHILDREN_SUM) {
			current->computed_size[axis] = 0.0f;
			for (UiWidgetId c = current->first_child; c != 0; ) {
				UiWidget *child = &h->widgets[c];
				if (axis == current->layout_axis) {
					current->computed_size[axis] += child->computed_size[axis];
				} else {
					// Keep track of the maximum size
					if (current->computed_size[axis] < child->computed_size[axis]) {
						current->computed_size[axis] = child->computed_size[axis];
					}
				}
				c = child->next;
			}
			// computed_size bounds all children. We need to add padding to get the final size.
			current->computed_size[axis] += 2.0f * current->padding;
		} else {
			current->computed_size[axis] = original_constraints[axis];
			current->computed_size[axis] += 2.0f * current->padding;
		}
	}
}

void ui_layout_end_frame(UiHierarchy *h, UiWidgetId root, struct Drawer2D *drawer)
{
	ASSERT(h->parent_stack_length == 0);

	// Layout widgets
	_ui_layout_traversal(h, root, drawer);

	// Garbage collect unused widgets
	for (uint32_t i = 0; i < ARRAY_LENGTH(h->widgets); ++i) {
		bool should_destroy = h->widgets[i].last_frame_touched_index < h->current_frame && h->widgets[i].key != 0;
		if (should_destroy) {
			// Remove widget from hash linked list
			UiWidgetId hash_prev = h->widgets[i].hash_prev;
			if (hash_prev) {
				h->widgets[hash_prev].hash_next = h->widgets[i].hash_next;
			}

			// Remove from widgets list
			ASSERT(h->widgets_length != 0);
			h->widgets_length -= 1;

			// Reset widget
			h->widgets[i] = (struct UiWidget){0};
		}
	}

	h->global_generation = 0;
	h->current_frame += 1;

	// inputs
	if (!h->inputs.mouse_is_down) {
		h->active_key = 0;
	} else {

	}
	h->hot_key = 0;
}

void _ui_imgui_rec(UiHierarchy *h, UiWidgetId root)
{
	UiWidget *current = &h->widgets[root];

	bool is_opened = ImGui_TreeNode(current->string);
	ImGui_SameLine();

	ImGui_Text("K[%llu]", current->key);

#if 0
	ImGui_Text("hash_prev %u", current->hash_prev);
	ImGui_Text("hash_next %u", current->hash_next);

	ImGui_Text("parent %u", current->parent);
	ImGui_Text("first_child %u", current->first_child);
	ImGui_Text("last_child %u", current->last_child);
	ImGui_Text("next %u", current->next);
	ImGui_Text("prev %u", current->prev);

	ImGui_Text("string %s", current->string);
#endif

	ImGui_SameLine();
	ImGui_Text("P[%fx%f]", current->computed_rel_position[0], current->computed_rel_position[1]);
	ImGui_SameLine();
	ImGui_Text("S[%fx%f]", current->computed_size[0], current->computed_size[1]);

	if (is_opened) {

		for (UiWidgetId c = current->first_child; c != 0; ) {
			_ui_imgui_rec(h, c);
			c = h->widgets[c].next;
		}

		ImGui_TreePop();
	}

}

void ui_imgui(UiHierarchy *h, UiWidgetId root)
{
	if (ImGui_Begin("Debug", NULL, 0)) {
		_ui_imgui_rec(h, root);
	}
	ImGui_End();
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
		h->widgets_length += 1;
	}
	// widget is at index
	h->widgets[index].flags = flags;
	h->widgets[index].semantic_size[0] = (UiSize){0};
	h->widgets[index].semantic_size[1] = (UiSize){0};
	h->widgets[index].layout_axis = 0;
	h->widgets[index].padding = 0.0f;
	h->widgets[index].string = string;
	h->widgets[index].display_string = NULL;
	h->widgets[index].display_string_length = 0;
	h->widgets[index].font_size = 0.0f;
	h->widgets[index].color = 0;

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
		// Link to parent's children
		UiWidgetId prev_sibling = h->widgets[p].last_child;
		h->widgets[index].prev = prev_sibling;
		h->widgets[index].next = 0;
		if (prev_sibling) {
			h->widgets[prev_sibling].next = index;
		}
		// Update parent's last and first child
		if (h->widgets[p].first_child == 0) {
			h->widgets[p].first_child = index;
		}
		h->widgets[p].last_child = index;
	}

	return index;
}

UiWidgetInputs ui_widget_behavior(UiHierarchy *h, UiWidgetId w)
{
	UiWidgetInputs result = {0};
	result.widget = w;
	result.mouse_position[0] = h->inputs.mouse_position[0];
	result.mouse_position[1] = h->inputs.mouse_position[1];
	float left = h->widgets[w].computed_abs_position[0];
	float width = h->widgets[w].computed_size[0];
	float top = h->widgets[w].computed_abs_position[1];
	float height = h->widgets[w].computed_size[1];
	bool mouse_in_x =  left <= result.mouse_position[0] && result.mouse_position[0] <= left + width;
	bool mouse_in_y =  top <= result.mouse_position[1] && result.mouse_position[1] <= top + height;

	UiWidgetFlags flags = h->widgets[w].flags;
	uint64_t key = h->widgets[w].key;

	result.hovered = mouse_in_x && mouse_in_y;
	if (result.hovered) {
		if ((flags & UI_WidgetFlag_Clickable) != 0) {
			h->hot_key = key;

			result.pressed = h->hot_key == key && h->inputs.mouse_is_down;
			if (result.pressed) {
				result.clicked = h->active_key != key;
				h->active_key = key;
			}

			result.released = h->hot_key == key && h->active_key == key && !h->inputs.mouse_is_down;
		}
	}

	return result;
}


// some other possible building parameterizations
void ui_widget_set_display_string(UiHierarchy *h, UiWidgetId widget, const char *string, uint32_t string_length, float font_size)
{
	h->widgets[widget].display_string = string;
	h->widgets[widget].display_string_length = string_length;
	h->widgets[widget].font_size = font_size;
}

void ui_widget_set_layout(UiHierarchy *h, UiWidgetId widget, int layout_axis, float padding)
{
	h->widgets[widget].layout_axis = layout_axis;
	h->widgets[widget].padding = padding;
}

void ui_widget_set_size_x(UiHierarchy *h, UiWidgetId widget, UiSize size)
{
	h->widgets[widget].semantic_size[0] = size;
}

void ui_widget_set_size_y(UiHierarchy *h, UiWidgetId widget, UiSize size)
{
	h->widgets[widget].semantic_size[1] = size;
}

void ui_widget_set_color(UiHierarchy *h, UiWidgetId widget, uint32_t color)
{
	h->widgets[widget].color = color;
}

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


void _ui_render_rec(UiHierarchy *h, UiWidgetId node, struct Drawer2D *drawer, float cursor_x, float cursor_y)
{
	UiWidget *current = &h->widgets[node];

	float top = cursor_y + current->computed_rel_position[1];
	float left = cursor_x + current->computed_rel_position[0];
	float width = current->computed_size[0];
	float height = current->computed_size[1];

	current->computed_abs_position[0] = left;
	current->computed_abs_position[1] = top;

	if ((current->flags & UI_WidgetFlag_DrawText) != 0) {

		struct DrawerTextInfo text_info = {0};
		text_info.size_px = current->font_size;
		text_info.color = current->color;
		drawer2d_draw_text(drawer, current->display_string, current->display_string_length, top, left, width, height, text_info);

	} else if (current->color != 0) {
		drawer2d_draw_rect(drawer, top, left, width, height, current->color);
	}


	cursor_x += current->computed_rel_position[0];
	cursor_y += current->computed_rel_position[1];
	for (UiWidgetId c = current->first_child; c != 0; ) {
		_ui_render_rec(h, c, drawer, cursor_x, cursor_y);
		c = h->widgets[c].next;
	}
}

void ui_render(UiHierarchy *h, UiWidgetId root, struct Drawer2D *drawer)
{
	_ui_render_rec(h, root, drawer, 0.0f, 0.0f);
}
