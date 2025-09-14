#pragma once

void clay_integration_init(int display_width, int display_height);
bool clay_integration_process_event(struct Application *application, SDL_Event* event);
void clay_integration_render(struct Drawer2D *drawer, Clay_RenderCommandArray *commands);
