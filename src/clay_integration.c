#define CLAY_IMPLEMENTATION
#include <clay.h>

#include "drawer2d.h"


Clay_Dimensions clay_integration_measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
	// Clay_TextElementConfig contains members such as fontId, fontSize, letterSpacing etc
	// Note: Clay_String->chars is not guaranteed to be null terminated
	return (Clay_Dimensions) {
		.width = text.length * config->fontSize, // <- this will only work for monospace fonts, see the renderers/ directory for more advanced text measurement
		.height = config->fontSize
	};
}

void clay_integration_error_callback(Clay_ErrorData errorData)
{
	// See the Clay_ErrorData struct for more information
	printf("%s", errorData.errorText.chars);
	switch(errorData.errorType) {
		// etc
	}
}

void clay_integration_init(int display_width, int display_height)
{
	// clay init
	// Note: malloc is only used here as an example, any allocator that provides
	// a pointer to addressable memory of at least totalMemorySize will work
	uint64_t totalMemorySize = Clay_MinMemorySize();
	Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
	// Note: screenWidth and screenHeight will need to come from your environment, Clay doesn't handle window related tasks
	Clay_Initialize(arena, (Clay_Dimensions) { display_width, display_height }, (Clay_ErrorHandler) { clay_integration_error_callback });
	Clay_SetMeasureTextFunction(clay_integration_measure_text, NULL);
	// Clay_SetDebugModeEnabled(true);
}

bool clay_integration_process_event(struct Application *application, SDL_Event* event)
{
	ImGuiIO* io = ImGui_GetIO();

	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
		{
			float mouse_pos_x = (float)event->motion.x;
			float mouse_pos_y = (float)event->motion.y;
		        // Optional: Update internal pointer position for handling mouseover / click / touch events - needed for scrolling & debug tools
			Clay_SetPointerState((Clay_Vector2) { mouse_pos_x, mouse_pos_y }, application->inputs.is_mouse_down);
			return true;
		}
	case SDL_EVENT_MOUSE_WHEEL:
		{
			//IMGUI_DEBUG_LOG("wheel %.2f %.2f, precise %.2f %.2f\n", (float)event->wheel.x, (float)event->wheel.y, event->wheel.preciseX, event->wheel.preciseY);
			float wheel_x = -event->wheel.x;
			float wheel_y = event->wheel.y;

			// Optional: Update internal pointer position for handling mouseover / click / touch events - needed for scrolling and debug tools
			Clay_UpdateScrollContainers(true, (Clay_Vector2) { wheel_x, wheel_y }, 0.16f);
			return true;
		}
	}
	return false;
}

static uint32_t clay_color_to_u32(Clay_Color color)
{
	uint32_t R = ((uint32_t)color.r) & 0xFF;
	uint32_t G = ((uint32_t)color.g) & 0xFF;
	uint32_t B = ((uint32_t)color.b) & 0xFF;
	uint32_t A = ((uint32_t)color.a) & 0xFF;
	return (A << 24) | (B << 16) | (G << 8) | R;
}

void clay_integration_render(struct Drawer2D *drawer, Clay_RenderCommandArray *commands)
{
	for (size_t i = 0; i < commands->length; i++) {
		Clay_RenderCommand *rcmd = Clay_RenderCommandArray_Get(commands, i);
		const Clay_BoundingBox bounding_box = rcmd->boundingBox;

		switch (rcmd->commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
			Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
			
			uint32_t color = clay_color_to_u32(config->backgroundColor);
			drawer2d_draw_rect(drawer, bounding_box.y, bounding_box.x, bounding_box.width, bounding_box.height, color);
			// SDL_SetRenderDrawBlendMode(data->renderer, SDL_BLENDMODE_BLEND);
			// SDL_SetRenderDrawColor(data->renderer, config->backgroundColor.r, config->backgroundColor.g, config->backgroundColor.b, config->backgroundColor.a);
			// if (config->cornerRadius.topLeft > 0) {
			// 	SDL_Clay_RenderFillRoundedRect(data, rect, config->cornerRadius.topLeft, config->backgroundColor);
			// } else {
			// 	SDL_RenderFillRect(data->renderer, &rect);
			// }
		} break;
		case CLAY_RENDER_COMMAND_TYPE_TEXT: {
			Clay_TextRenderData *config = &rcmd->renderData.text;

			uint32_t color = clay_color_to_u32(config->textColor);
			drawer2d_draw_rect(drawer, bounding_box.y, bounding_box.x, bounding_box.width, bounding_box.height, color);

			// TTF_Font *font = data->fonts[config->fontId];
			// TTF_SetFontSize(font, config->fontSize);
			// TTF_Text *text = TTF_CreateText(data->textEngine, font, config->stringContents.chars, config->stringContents.length);
			// TTF_SetTextColor(text, config->textColor.r, config->textColor.g, config->textColor.b, config->textColor.a);
			// TTF_DrawRendererText(text, rect.x, rect.y);
			// TTF_DestroyText(text);
		} break;
		case CLAY_RENDER_COMMAND_TYPE_BORDER: {
			Clay_BorderRenderData *config = &rcmd->renderData.border;

			float min_radius = SDL_min(bounding_box.width, bounding_box.height) / 2.0f;
			Clay_CornerRadius clamped_radius = {
				.topLeft = SDL_min(config->cornerRadius.topLeft, min_radius),
				.topRight = SDL_min(config->cornerRadius.topRight, min_radius),
				.bottomLeft = SDL_min(config->cornerRadius.bottomLeft, min_radius),
				.bottomRight = SDL_min(config->cornerRadius.bottomRight, min_radius)
			};
			uint32_t color = clay_color_to_u32(config->color);
			
			//edges
			if (config->width.left > 0) {
				float left = bounding_box.x - 1.0f;
				float top = bounding_box.y + clamped_radius.topLeft;
				float width = config->width.left;
				float height = bounding_box.height - clamped_radius.topLeft - clamped_radius.bottomLeft;
				drawer2d_draw_rect(drawer, top, left, width, height, color);
			}
			// top right bottom

			//corners
			// if (config->cornerRadius.topLeft > 0) {
			//	const float centerX = rect.x + clampedRadii.topLeft -1;
			//	const float centerY = rect.y + clampedRadii.topLeft - 1;
			//	SDL_Clay_RenderArc(data, (SDL_FPoint){centerX, centerY}, clampedRadii.topLeft,
			//			   180.0f, 270.0f, config->width.top, config->color);
			//}
			// topright bottomleft bottomright
		} break;
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
			Clay_BoundingBox boundingBox = rcmd->boundingBox;
			// currentClippingRectangle = (SDL_Rect) {
			//	.x = boundingBox.x,
			//	.y = boundingBox.y,
			//	.w = boundingBox.width,
			//	.h = boundingBox.height,
			// };
			// SDL_SetRenderClipRect(data->renderer, &currentClippingRectangle);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
			// SDL_SetRenderClipRect(data->renderer, NULL);
			break;
		}
		case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
			Clay_RectangleRenderData *config = &rcmd->renderData.rectangle;
			
			uint32_t color = 0xFF0000FF;
			drawer2d_draw_rect(drawer, bounding_box.y, bounding_box.x, bounding_box.width, bounding_box.height, color);
			
			// SDL_Texture *texture = (SDL_Texture *)rcmd->renderData.image.imageData;
			// const SDL_FRect dest = { rect.x, rect.y, rect.w, rect.h };
			// SDL_RenderTexture(data->renderer, texture, NULL, &dest);
			break;
		}
		default:
			SDL_Log("Unknown render command type: %d", rcmd->commandType);
		}
	}
}
