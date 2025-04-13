#include "renderer.h"
#include "file.h"

struct Renderer
{
	VulkanDevice *device;
	uint32_t output_rt;
	
	uint32_t imgui_pso;
	uint32_t imgui_fontatlas;
	uint32_t imgui_ibuffer;
	uint32_t imgui_vbuffer;
};

uint32_t renderer_get_size(void)
{
	return sizeof(Renderer);
}

void renderer_init(Renderer *renderer, SDL_Window *window)
{
	renderer->device = calloc(1, vulkan_get_device_size());

	// Init renderer
	SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
	void *hwnd = SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	vulkan_create_device(renderer->device, hwnd);

	renderer->output_rt = 0;
	new_render_target(renderer->device, 0,
			  renderer->device->swapchain_width,
			  renderer->device->swapchain_height,
			  PG_FORMAT_R8G8B8A8_UNORM);

	// Create imgui assets
	renderer->imgui_pso = 0;
	renderer->imgui_ibuffer = 0;
	renderer->imgui_vbuffer = 1;
	struct MaterialAsset imgui_material = {0};
	Serializer s = serialize_read_file("cooking/imgui.mat.json");
	Serialize_MaterialAsset(&s, &imgui_material);
	new_graphics_program(renderer->device, renderer->imgui_pso, imgui_material);
	new_index_buffer(renderer->device, renderer->imgui_ibuffer, (64 << 10));
	new_storage_buffer(renderer->device, renderer->imgui_vbuffer, (64 << 10));

	unsigned char *pixels = NULL;
	int width = 0;
	int height = 0;
	int bpp = 0;
	ImGuiIO *io = ImGui_GetIO();
	ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &pixels, &width, &height, &bpp);

}

static void renderer_imgui_pass(Renderer *renderer, VulkanFrame *frame, VulkanRenderPass *pass)
{
	ImGui_Render();
	ImDrawData *draw_data = ImGui_GetDrawData();
	
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0) {
		return;
	}

	// Upload vertices
	uint32_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
	uint32_t index_size  = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
	uint32_t vbuffer_size = buffer_get_size(renderer->device, renderer->imgui_vbuffer);
	uint32_t ibuffer_size = buffer_get_size(renderer->device, renderer->imgui_ibuffer);
	if (vertex_size > vbuffer_size || index_size > ibuffer_size) {
		return;
	}
	ImDrawVert *vertices_gpu = buffer_get_mapped_pointer(renderer->device, renderer->imgui_vbuffer);
	ImDrawIdx *indices_gpu = buffer_get_mapped_pointer(renderer->device, renderer->imgui_ibuffer);
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* draw_list = draw_data->CmdLists.Data[n];
		memcpy(vertices_gpu, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(indices_gpu, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertices_gpu += draw_list->VtxBuffer.Size;
		indices_gpu += draw_list->IdxBuffer.Size;
	}

	// Render
	struct ImGuiPushConstants
	{
		float scale[2];
		float translation[2];
		uint64_t vbuffer;
	} constants;
	constants.scale[0] = 2.0f / draw_data->DisplaySize.x;
	constants.scale[1] = 2.0f / draw_data->DisplaySize.y;
	constants.translation[0] = -1.0f - draw_data->DisplayPos.x * constants.scale[0];
	constants.translation[1] = -1.0f - draw_data->DisplayPos.y * constants.scale[1];
	constants.vbuffer = buffer_get_gpu_address(renderer->device, renderer->imgui_vbuffer);
	vulkan_push_constants(renderer->device, pass, &constants, sizeof(constants));
	vulkan_bind_index_buffer(renderer->device, pass, renderer->imgui_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, pass, renderer->imgui_pso);
	
	// Will project scissor/clipping rectangles into framebuffer space
	ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
	ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)
	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++) {
		const ImDrawList* draw_list = draw_data->CmdLists.Data[n];
		for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
			const ImDrawCmd* pcmd = &draw_list->CmdBuffer.Data[cmd_i];
			if (pcmd->UserCallback != NULL){
				pcmd->UserCallback(draw_list, pcmd);
				continue;
			}
			
			// Project scissor/clipping rectangles into framebuffer space
			ImVec2 clip_min;
			clip_min.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
			clip_min.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
			ImVec2 clip_max;
			clip_max.x = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
			clip_max.y = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

			// Clamp to viewport as SDL_SetGPUScissor() won't accept values that are off bounds
			if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
			if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
			if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
			if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
				continue;

			// Apply scissor/clipping rectangle
			struct VulkanScissor scissor = {};
			scissor.x = (uint32_t)clip_min.x;
			scissor.y = (uint32_t)clip_min.y;
			scissor.w = (uint32_t)(clip_max.x - clip_min.x);
			scissor.h = (uint32_t)(clip_max.y - clip_min.y);
			vulkan_set_scissor(renderer->device, pass, scissor);

			// Bind DescriptorSet with font or user texture
			// SDL_BindGPUFragmentSamplers(render_pass, 0, (SDL_GPUTextureSamplerBinding*)pcmd->GetTexID(), 1);
			// Draw
			struct VulkanDraw draw = {pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0};
			vulkan_draw(renderer->device, pass, draw);
		}
		global_idx_offset += draw_list->IdxBuffer.Size;
		global_vtx_offset += draw_list->VtxBuffer.Size;
	}
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

	union VulkanClearColor clear_color = {0};
	vulkan_clear(renderer->device, &pass, &clear_color, 1, 0.0f);

	renderer_imgui_pass(renderer, &frame, &pass);
	
	end_render_pass(renderer->device, &pass);

	end_frame(renderer->device, &frame, renderer->output_rt);
}
