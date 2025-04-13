#pragma once

typedef struct Renderer Renderer;

uint32_t renderer_get_size(void);
void renderer_init(Renderer *renderer, SDL_Window *window);
void renderer_render(Renderer *renderer);
