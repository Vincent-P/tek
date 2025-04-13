#include "renderer.h"
#include "file.h"

struct Renderer
{
	VulkanDevice *device;
	uint32_t output_rt;
};

uint32_t renderer_get_size(void)
{
	return sizeof(Renderer);
}

void renderer_init(Renderer *renderer, SDL_Window *window)
{
	renderer->device = calloc(1, vulkan_get_device_size());

	SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
	void *hwnd = SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	vulkan_create_device(renderer->device, hwnd);

	renderer->output_rt = 0;
	new_render_target(renderer->device, 0,
			  renderer->device->swapchain_width,
			  renderer->device->swapchain_height,
			  PG_FORMAT_R8G8B8A8_UNORM);

	uint32_t imgui_pso = 0;
	struct MaterialAsset imgui_material = {0};
	Serializer s = serialize_read_file("cooking/imgui.mat.json");
	Serialize_MaterialAsset(&s, &imgui_material);
	new_graphics_program(renderer->device, imgui_pso, imgui_material);
}

void renderer_render(Renderer *renderer)
{
	VulkanFrame frame = {0};

	uint32_t swapchain_width = 0;
	uint32_t swapchain_height = 0;
	begin_frame(renderer->device, &frame, &swapchain_width, &swapchain_height);

	if (renderer->device->rts[renderer->output_rt].width != swapchain_width || renderer->device->rts[renderer->output_rt].height != swapchain_height) {
		resize_render_target(renderer->device, renderer->output_rt, swapchain_width, swapchain_height);
	}

	VulkanRenderPass pass = {0};
	struct VulkanBeginPassInfo pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_UI, {renderer->output_rt}, 1};
	begin_render_pass(renderer->device, &frame, &pass, pass_info);
	
	end_render_pass(renderer->device, &pass);

	end_frame(renderer->device, &frame, renderer->output_rt);
}
