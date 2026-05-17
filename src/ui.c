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

void _ui_layout_node(UiHierarchy *h, UiWidgetId node, bool is_leaf)
{
	(void)h;
	(void)node;
	(void)is_leaf;
	if (is_leaf) {
	} else {
		// if we are an internal node, we need to
	}
}

float _measure_text(const char *s, int axis)
{
	if (axis == 0) {
		return strlen(s) * 10.0f;
	} else {
		return 10.0f;
	}
}

void _ui_layout_traversal(UiHierarchy *h, UiWidgetId node)
{
	UiWidget *current = &h->widgets[node];
	bool is_leaf = current->last_child == 0;
	if (is_leaf) {

		for (int axis = 0; axis < 2; ++axis) {
			// if we are a leaf, the size of the node is the size of its content, easy!
			if (current->semantic_size[axis].kind == UI_SIZE_KIND_PIXELS) {
				current->computed_size[axis] = current->semantic_size[axis].value;
			} else if (current->semantic_size[axis].kind == UI_SIZE_KIND_TEXT) {
				current->computed_size[axis] = _measure_text(current->string, axis);
			} else if (current->semantic_size[axis].kind == UI_SIZE_KIND_PERCENT) {
				// parent size constraints are already written in computed_size
				current->computed_size[axis] *= current->semantic_size[axis].value;
			} else {
				// NULL, CHILDREN_SUM, FLEX
			}
		}

	} else {
		ASSERT(current->layout_axis <= 1);

		// First we need to set our constraints if we are forced to a pixel size
		for (int axis = 0; axis < 2; ++axis) {
			if (current->semantic_size[axis].kind == UI_SIZE_KIND_PIXELS) {
				current->computed_size[axis] = current->semantic_size[axis].value;
			}
		}
		float original_constraints[2] = {0};
		original_constraints[0] = current->computed_size[0];
		original_constraints[1] = current->computed_size[1];

		// Compute sizes of all non-flex children
		for (UiWidgetId c = current->first_child; c != 0; ) {

			UiWidget *child = &h->widgets[c];
			bool is_child_flex = child->semantic_size[0].kind == UI_SIZE_KIND_FLEX
				|| child->semantic_size[1].kind == UI_SIZE_KIND_FLEX;
			if (!is_child_flex) {
				// Set size constrainst on children from the parent
				child->computed_size[0] = current->computed_size[0];
				child->computed_size[1] = current->computed_size[1];

				_ui_layout_traversal(h, c);

				// When a child has been laid out, update constraints
				current->computed_size[current->layout_axis] -= child->computed_size[current->layout_axis];
			}
			c = child->next;
		}

		// Compute sizes of flex children with remaining size
		float max_flex[2] = {0.0f, 0.0f};
		for (UiWidgetId c = current->first_child; c != 0; ) {
			UiWidget *child = &h->widgets[c];
			if (child->semantic_size[0].kind == UI_SIZE_KIND_FLEX) {
				max_flex[0] += child->semantic_size[0].value;
			}
			if (child->semantic_size[1].kind == UI_SIZE_KIND_FLEX) {
				max_flex[1] += child->semantic_size[1].value;
			}
			c = child->next;
		}
		for (UiWidgetId c = current->first_child; c != 0; ) {

			UiWidget *child = &h->widgets[c];
			bool is_child0_flex = child->semantic_size[0].kind == UI_SIZE_KIND_FLEX;
			bool is_child1_flex = child->semantic_size[1].kind == UI_SIZE_KIND_FLEX;
			if (is_child0_flex || is_child1_flex) {
				// Set size constrainst on children from the parent
				float flex0_factor = child->semantic_size[0].value / max_flex[0];
				float flex1_factor = child->semantic_size[1].value / max_flex[1];
				child->computed_size[0] = is_child0_flex ? current->computed_size[0] * flex0_factor : current->computed_size[0];
				child->computed_size[1] = is_child1_flex ? current->computed_size[1] * flex1_factor : current->computed_size[1];

				_ui_layout_traversal(h, c);

				// When a child has been laid out, update constraints
				if (child->semantic_size[current->layout_axis].kind != UI_SIZE_KIND_FLEX) {
					current->computed_size[current->layout_axis] -= child->computed_size[current->layout_axis];
				}
			}
			c = child->next;
		}

		// now all children have computed their size, position them

		// for now, our size is sum of children in layout axis, and maximum in other axis
		current->computed_size[0] = 0.0f;
		current->computed_size[1] = 0.0f;
		for (UiWidgetId c = current->first_child; c != 0; ) {
			UiWidget *child = &h->widgets[c];
			for (int axis = 0; axis < 2; ++axis) {
				if (axis == current->layout_axis) {
					current->computed_size[axis] += child->computed_size[axis];
				} else {
					if (current->computed_size[axis] < child->computed_size[axis]) {
						current->computed_size[axis] = child->computed_size[axis];
					}
				}
			}
			c = child->next;
		}
	}
}

void ui_layout_end_frame(UiHierarchy *h, UiWidgetId root)
{
	ASSERT(h->parent_stack_length == 0);

	// Layout widgets
	_ui_layout_traversal(h, root);

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
}

void _ui_imgui_rec(UiHierarchy *h, UiWidgetId root)
{
	UiWidget *current = &h->widgets[root];

	if (ImGui_TreeNode(current->string)) {
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

// some other possible building parameterizations
void ui_widget_set_display_string(UiHierarchy *h, UiWidgetId widget, const char *string)
{
	h->widgets[widget].string = string;
}

void ui_widget_set_child_layout_axis(UiHierarchy *h, UiWidgetId widget, int axis)
{
	h->widgets[widget].layout_axis = axis;
}

void ui_widget_set_size_x(UiHierarchy *h, UiWidgetId widget, UiSize size)
{
	h->widgets[widget].semantic_size[0] = size;
}

void ui_widget_set_size_y(UiHierarchy *h, UiWidgetId widget, UiSize size)
{
	h->widgets[widget].semantic_size[1] = size;
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
