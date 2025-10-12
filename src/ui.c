static char ui_mem[1024];
static Arena ui_arena;

void ui_new_frame()
{
	ui_arena.beg = ui_mem;
	ui_arena.end = ui_mem + sizeof(ui_mem);
}

static void ui_handle_button_interaction(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
	bool *pressed = (bool *)userData;
	// Pointer state allows you to detect mouse down / hold / release
	if (pressed != NULL && pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
		*pressed = true;
	}
}

void ui_button(const char *label, bool *clicked)
{
	Clay_String label_string = (struct Clay_String) {.isStaticallyAllocated = false, .length = strlen(label), .chars = label};
	
	Clay_TextElementConfig *label_text_config = CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = {255, 255, 255, 255}, .textAlignment = CLAY_TEXT_ALIGN_RIGHT });

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
