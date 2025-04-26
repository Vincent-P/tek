#include "renderer.h"
#include "file.h"

#define RENDERER_SKELETAL_MESH_CAPACITY (8)
#define RENDERER_MESH_VERTEX_CAPACITY (64 << 10)
#define RENDERER_MESH_INDEX_CAPACITY (64 << 10)


struct DdVert
{
	Float3 pos;
	uint32_t col;
};

struct MeshVert
{
	Float3 position;
	uint32_t bone_indices;
	Float3 normals;
	uint32_t bone_weights;
	// Float3 tangents
	// Float2 uvs
	// uint32_t colors
};

struct SkeletalMeshCBuffer
{
	Float3x4 bone_matrices[MAX_BONES_PER_MESH];
};

struct SkeletalMeshConstants
{
	Float4x4 proj;
	Float3x4 view;
	Float3x4 transform;
	uint64_t bones_matrices_buffer;
	uint64_t vbuffer;
};

struct RenderSkeletalMesh
{
	oa_allocation_t vbuffer_allocation;
	oa_allocation_t ibuffer_allocation;
	uint32_t index_count;
	uint32_t first_index;
	int32_t vertex_offset;
};

struct Renderer
{
	VulkanDevice *device;
	uint32_t output_rt;
	uint32_t depth_rt;
	// imgui
	uint32_t imgui_pso;
	uint32_t imgui_fontatlas;
	uint32_t imgui_ibuffer;
	uint32_t imgui_vbuffer;
	// debug draw
	uint32_t dd_pso;
	uint32_t dd_line_pso;
	uint32_t dd_ibuffer;
	uint32_t dd_vbuffer;
	// 3d meshes
	uint32_t mesh_pso;
	oa_allocator_t mesh_vbuffer_allocator;
	oa_allocator_t mesh_ibuffer_allocator;
	uint32_t mesh_vbuffer;
	uint32_t mesh_ibuffer;
	RenderSkeletalMesh skeletal_meshes[RENDERER_SKELETAL_MESH_CAPACITY];
	struct SkeletalMeshInstanceData skeletal_mesh_instances[2];
	uint32_t skeletal_mesh_instances_length;
	// background shaders
	uint32_t bg0_pso;
	// context
	Float4x4 proj;
	Float4x4 invproj;
	Float3x4 view;
	Float3x4 invview;
	float time;
	uint32_t constant_buffer;
	uint32_t constant_offset;
};

uint32_t renderer_get_size(void)
{
	return sizeof(Renderer);
}

void renderer_init(Renderer *renderer, struct AssetLibrary *assets, SDL_Window *window)
{
	renderer->device = calloc(1, vulkan_get_device_size());

	// Init renderer
	SDL_PropertiesID window_props = SDL_GetWindowProperties(window);
	void *hwnd = SDL_GetPointerProperty(window_props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	vulkan_create_device(renderer->device, hwnd);

	renderer->output_rt = 0;
	new_render_target(renderer->device, renderer->output_rt,
			  renderer->device->swapchain_width,
			  renderer->device->swapchain_height,
			  PG_FORMAT_R8G8B8A8_UNORM);
	
	renderer->depth_rt = 1;
	new_render_target(renderer->device, renderer->depth_rt,
			  renderer->device->swapchain_width,
			  renderer->device->swapchain_height,
			  PG_FORMAT_D32_SFLOAT);

	// Create imgui resources
	renderer->imgui_pso = 0;
	renderer->imgui_fontatlas = 0;
	renderer->imgui_ibuffer = 0;
	renderer->imgui_vbuffer = 1;
	struct MaterialAsset const *imgui_material = asset_library_get_material(assets, 3661877039);
	new_graphics_program(renderer->device, renderer->imgui_pso, *imgui_material);
	new_index_buffer(renderer->device, renderer->imgui_ibuffer, (1 << 20));
	new_storage_buffer(renderer->device, renderer->imgui_vbuffer, (1 << 20));
	unsigned char *pixels = NULL;
	int width = 0;
	int height = 0;
	int bpp = 0;
	ImGuiIO *io = ImGui_GetIO();
	ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &pixels, &width, &height, &bpp);
	new_texture(renderer->device, renderer->imgui_fontatlas, width, height, PG_FORMAT_R8G8B8A8_UNORM, pixels, width*height*bpp);
	
	// Create debug draw resources
	struct VulkanGraphicsPsoSpec pso_spec = {0};
	pso_spec.topology = VULKAN_TOPOLOGY_TRIANGLE_LIST;
	pso_spec.fillmode = VULKAN_FILL_MODE_LINE;
	renderer->dd_pso = 1;
	renderer->dd_line_pso = 2;
	renderer->dd_ibuffer = 2;
	renderer->dd_vbuffer = 3;
	struct MaterialAsset const *dd_material = asset_library_get_material(assets, 3200094153);
	new_graphics_program_ex(renderer->device, renderer->dd_pso, *dd_material, pso_spec);
	pso_spec.topology = VULKAN_TOPOLOGY_LINE_LIST;
	new_graphics_program_ex(renderer->device, renderer->dd_line_pso, *dd_material, pso_spec);
	new_index_buffer(renderer->device, renderer->dd_ibuffer, (64 << 10));
	new_storage_buffer(renderer->device, renderer->dd_vbuffer, (64 << 10));

	// Create mesh resources
	uint32_t MAX_MESH_ALLOCATIONS = 32;
	int res = oa_create(&renderer->mesh_vbuffer_allocator, RENDERER_MESH_VERTEX_CAPACITY, MAX_MESH_ALLOCATIONS);
	assert(res == 0);
	res = oa_create(&renderer->mesh_ibuffer_allocator, RENDERER_MESH_INDEX_CAPACITY, MAX_MESH_ALLOCATIONS);
	assert(res == 0);
	renderer->mesh_pso = 3;
	renderer->mesh_ibuffer = 4;
	renderer->mesh_vbuffer = 5;
	new_index_buffer(renderer->device, renderer->mesh_ibuffer, RENDERER_MESH_INDEX_CAPACITY * sizeof(uint32_t));
	new_storage_buffer(renderer->device, renderer->mesh_vbuffer, RENDERER_MESH_VERTEX_CAPACITY * sizeof(struct MeshVert));
	struct MaterialAsset const *mesh_material = asset_library_get_material(assets, 2455425701);
	new_graphics_program(renderer->device, renderer->mesh_pso, *mesh_material);

	memset(buffer_get_mapped_pointer(renderer->device, renderer->mesh_vbuffer), 0, buffer_get_size(renderer->device, renderer->mesh_vbuffer));
	memset(buffer_get_mapped_pointer(renderer->device, renderer->mesh_ibuffer), 0, buffer_get_size(renderer->device, renderer->mesh_ibuffer));

	renderer->constant_buffer = 6;
	new_storage_buffer(renderer->device, renderer->constant_buffer, (64 << 10));

	// shader bg
	struct MaterialAsset const *bg0_material = asset_library_get_material(assets, 295627051);
	renderer->bg0_pso = 4;
	new_graphics_program(renderer->device, renderer->bg0_pso, *bg0_material);
}

void renderer_set_time(Renderer *renderer, float t)
{
	renderer->time = t;
}

void renderer_create_render_skeletal_mesh(Renderer *renderer, struct SkeletalMeshAsset *asset, uint32_t handle)
{
	oa_allocation_t vbuffer_allocation = {0};
	int alloc_res = oa_allocate(&renderer->mesh_vbuffer_allocator, asset->vertices_length, &vbuffer_allocation);
	assert(alloc_res == 0);
	oa_allocation_t ibuffer_allocation = {0};
	alloc_res = oa_allocate(&renderer->mesh_ibuffer_allocator, asset->indices_length, &ibuffer_allocation);
	assert(alloc_res == 0);
	
	renderer->skeletal_meshes[handle].vbuffer_allocation = vbuffer_allocation;
	renderer->skeletal_meshes[handle].ibuffer_allocation = ibuffer_allocation;
	renderer->skeletal_meshes[handle].index_count = asset->indices_length;
	renderer->skeletal_meshes[handle].first_index = ibuffer_allocation.offset;
	renderer->skeletal_meshes[handle].vertex_offset = vbuffer_allocation.offset;

	struct MeshVert *gpu_vertices = buffer_get_mapped_pointer(renderer->device, renderer->mesh_vbuffer);
	uint32_t *gpu_indices = buffer_get_mapped_pointer(renderer->device, renderer->mesh_ibuffer);

	for (uint32_t i = 0; i < asset->vertices_length; ++i) {		
		gpu_vertices[vbuffer_allocation.offset + i].position = asset->vertices_positions[i];
		gpu_vertices[vbuffer_allocation.offset + i].bone_indices = asset->vertices_bone_indices[i];
		gpu_vertices[vbuffer_allocation.offset + i].normals = asset->vertices_normals[i];
		gpu_vertices[vbuffer_allocation.offset + i].bone_weights = asset->vertices_bone_weights[i];
	}
	for (uint32_t i = 0; i < asset->indices_length; ++i) {
		gpu_indices[ibuffer_allocation.offset + i] = asset->indices[i];
	}

	asset->render_handle = handle;
}

void renderer_register_skeletal_mesh_instance(Renderer *renderer, struct SkeletalMeshInstanceData data)
{
	uint32_t iinstance = renderer->skeletal_mesh_instances_length;
	assert(iinstance < ARRAY_LENGTH(renderer->skeletal_mesh_instances));

	data.mesh_render_handle = data.mesh->render_handle;
	
	renderer->skeletal_mesh_instances[iinstance] = data;
	renderer->skeletal_mesh_instances_length += 1;
}

void renderer_clear_skeletal_mesh_instances(Renderer *renderer)
{
	renderer->skeletal_mesh_instances_length = 0;
}

static void renderer_debug_draw_pass(Renderer *renderer, VulkanFrame *frame, VulkanRenderPass *pass)
{
	uint32_t vertex_size = g_dd.points_length * 6 * sizeof(struct DdVert); //  6 vertices
	uint32_t index_size = g_dd.points_length * 8 * 3 * sizeof(uint32_t); // 8 faces

	vertex_size += g_dd.lines_length + 2 * sizeof(struct DdVert);
	index_size += g_dd.lines_length + 2 * sizeof(uint32_t);

	uint32_t vbuffer_size = buffer_get_size(renderer->device, renderer->dd_vbuffer);
	uint32_t ibuffer_size = buffer_get_size(renderer->device, renderer->dd_ibuffer);
	if (vertex_size > vbuffer_size || index_size > ibuffer_size) {
		return;
	}

	struct DdVert *vertices = buffer_get_mapped_pointer(renderer->device, renderer->dd_vbuffer);
	uint32_t *indices = buffer_get_mapped_pointer(renderer->device, renderer->dd_ibuffer);
		
	// generate spheres
	float point_radius = 0.05f;
	uint32_t vertex_count = 0;
	uint32_t index_count = 0;

	struct VulkanDraw spheres_draw = {0};
	spheres_draw.instance_count = 1;
	spheres_draw.first_index = index_count;
	for (uint32_t ipoint = 0; ipoint < g_dd.points_length; ++ipoint) {
		vertices[vertex_count+0].pos = float3_add(g_dd.points[ipoint], (Float3){-point_radius, 0.0f, -point_radius});
		vertices[vertex_count+0].col = 0x400000ff;
		vertices[vertex_count+1].pos = float3_add(g_dd.points[ipoint], (Float3){-point_radius, 0.0f, point_radius});
		vertices[vertex_count+1].col = 0x400000ff;
		vertices[vertex_count+2].pos = float3_add(g_dd.points[ipoint], (Float3){point_radius, 0.0f, -point_radius});
		vertices[vertex_count+2].col = 0x400000ff;
		vertices[vertex_count+3].pos = float3_add(g_dd.points[ipoint], (Float3){point_radius, 0.0f, point_radius});
		vertices[vertex_count+3].col = 0x400000ff;
		vertices[vertex_count+4].pos = float3_add(g_dd.points[ipoint], (Float3){0.0f, -point_radius, 0.0f});
		vertices[vertex_count+4].col = 0x400000ff;
		vertices[vertex_count+5].pos = float3_add(g_dd.points[ipoint], (Float3){0.0f, point_radius, 0.0f});
		vertices[vertex_count+5].col = 0x400000ff;

		indices[index_count++] = vertex_count+0;
		indices[index_count++] = vertex_count+1;
		indices[index_count++] = vertex_count+4;
		
		indices[index_count++] = vertex_count+0;
		indices[index_count++] = vertex_count+1;
		indices[index_count++] = vertex_count+5;
		
		indices[index_count++] = vertex_count+3;
		indices[index_count++] = vertex_count+1;
		indices[index_count++] = vertex_count+4;
		
		indices[index_count++] = vertex_count+3;
		indices[index_count++] = vertex_count+1;
		indices[index_count++] = vertex_count+5;
		
		indices[index_count++] = vertex_count+3;
		indices[index_count++] = vertex_count+2;
		indices[index_count++] = vertex_count+4;
		
		indices[index_count++] = vertex_count+3;
		indices[index_count++] = vertex_count+2;
		indices[index_count++] = vertex_count+5;
			
		vertex_count += 6;
	}
	spheres_draw.index_count = index_count - spheres_draw.first_index;

	// generate lines
	struct VulkanDraw lines_draw = {0};
	lines_draw.instance_count = 1;
	lines_draw.first_index = index_count;
	for (uint32_t iline = 0; iline < g_dd.lines_length; ++iline) {
		vertices[vertex_count+0].pos = g_dd.lines_from[iline];
		vertices[vertex_count+0].col = g_dd.lines_col[iline];
		vertices[vertex_count+1].pos = g_dd.lines_to[iline];
		vertices[vertex_count+1].col = g_dd.lines_col[iline];

		indices[index_count++] = vertex_count+0;
		indices[index_count++] = vertex_count+1;
		
		vertex_count += 2;
	}
	lines_draw.index_count = index_count - lines_draw.first_index;

	// Render
	struct DdPushConstants
	{
		Float4x4 proj;
		Float3x4 view;
		uint64_t vbuffer;
	} constants;
	constants.proj = renderer->proj;
	constants.view = renderer->view;
	constants.vbuffer = buffer_get_gpu_address(renderer->device, renderer->dd_vbuffer);
	
	vulkan_push_constants(renderer->device, pass, &constants, sizeof(constants));
	vulkan_bind_index_buffer(renderer->device, pass, renderer->dd_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, pass, renderer->dd_pso);

	vulkan_insert_debug_label(renderer->device, pass, "dd: spheres");
	vulkan_draw(renderer->device, pass, spheres_draw);
	
	vulkan_bind_graphics_pso(renderer->device, pass, renderer->dd_line_pso);
	
	vulkan_insert_debug_label(renderer->device, pass, "dd: lines");
	vulkan_draw(renderer->device, pass, lines_draw);
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
	vulkan_insert_debug_label(renderer->device, pass, "imgui");

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

void renderer_render(Renderer *renderer, struct Camera* camera)
{
	TracyCZoneN(f, "Renderer render", true);
	
	VulkanFrame frame = {0};

	uint32_t swapchain_width = 0;
	uint32_t swapchain_height = 0;
	begin_frame(renderer->device, &frame, &swapchain_width, &swapchain_height);

	renderer->proj = perspective_projection(camera->vertical_fov, (float)swapchain_width / (float)swapchain_height, 1.0f, 100.0f, &renderer->invproj);
	renderer->view = lookat_view(camera->position, camera->lookat, &renderer->invview);
	
	vulkan_bind_texture(renderer->device, &frame, renderer->imgui_fontatlas, 0);

	if (renderer->device->rts[renderer->output_rt].width != swapchain_width || renderer->device->rts[renderer->output_rt].height != swapchain_height) {
		resize_render_target(renderer->device, renderer->output_rt, swapchain_width, swapchain_height);
		resize_render_target(renderer->device, renderer->depth_rt, swapchain_width, swapchain_height);
	}

	VulkanRenderPass pass = {0};
	union VulkanClearColor clear_color = {0};
	
	struct VulkanBeginPassInfo mesh_pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_MESH, {renderer->output_rt}, 1, renderer->depth_rt};
	begin_render_pass_discard(renderer->device, &frame, &pass, mesh_pass_info);
	vulkan_clear(renderer->device, &pass, &clear_color, 1, 0.0f);

	{
		struct Bg0Constants
		{
			Float4x4 proj;
			Float4x4 invproj;
			Float3x4 view;
			Float3x4 invview;
			Float3 resolution;
			float time;
		} constants;
		constants.proj = renderer->proj;
		constants.invproj = renderer->invproj;
		constants.view = renderer->view;
		constants.invview = renderer->invview;
		constants.resolution = (Float3){swapchain_width, swapchain_height, 1};
		constants.time = renderer->time;
		
		vulkan_insert_debug_label(renderer->device, &pass, "background");
		vulkan_bind_graphics_pso(renderer->device, &pass, renderer->bg0_pso);
		vulkan_push_constants(renderer->device, &pass, &constants, sizeof(constants));
		vulkan_draw_not_indexed(renderer->device, &pass, 3);
	}

	vulkan_insert_debug_label(renderer->device, &pass, "skeletal meshes");
	vulkan_bind_index_buffer(renderer->device, &pass, renderer->mesh_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, &pass, renderer->mesh_pso);
	for (uint32_t iinstance = 0; iinstance < renderer->skeletal_mesh_instances_length; ++iinstance){
		struct RenderSkeletalMesh *render_mesh = &renderer->skeletal_meshes[renderer->skeletal_mesh_instances[iinstance].mesh_render_handle];
		struct SkeletalMeshInstance const *dynamic_data_mesh = renderer->skeletal_mesh_instances[iinstance].dynamic_data_mesh;
		Float3x4 const* dynamic_data_transform = &renderer->skeletal_mesh_instances[iinstance].dynamic_data_spatial->world_transform;
		// upload bone matrices
		char *gpu_data = buffer_get_mapped_pointer(renderer->device, renderer->constant_buffer);
		gpu_data += renderer->constant_offset;
		assert(renderer->constant_offset + sizeof(struct SkeletalMeshCBuffer) <= buffer_get_size(renderer->device, renderer->constant_buffer));
		struct SkeletalMeshCBuffer *gpu_cbuffer = (struct SkeletalMeshCBuffer*)gpu_data;
		memcpy(gpu_cbuffer, dynamic_data_mesh->pose, sizeof(dynamic_data_mesh->pose));
		// prepare constants
		struct SkeletalMeshConstants constants = {0};
		constants.proj = renderer->proj;
		constants.view = renderer->view;
		constants.transform = *dynamic_data_transform;
		constants.bones_matrices_buffer = buffer_get_gpu_address(renderer->device, renderer->constant_buffer) + renderer->constant_offset;
		constants.vbuffer = buffer_get_gpu_address(renderer->device, renderer->mesh_vbuffer);	
		vulkan_push_constants(renderer->device, &pass, &constants, sizeof(constants));
		// draw
		struct VulkanDraw draw = {render_mesh->index_count,
					  1,
					  render_mesh->first_index,
					  render_mesh->vertex_offset,
					  0};
		vulkan_draw(renderer->device, &pass, draw);
		renderer->constant_offset += sizeof(struct SkeletalMeshCBuffer);

	}
	renderer->constant_offset = 0;
	end_render_pass(renderer->device, &pass);

	struct VulkanBeginPassInfo dd_pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_DEBUG_DRAW, {renderer->output_rt}, 1};
	begin_render_pass(renderer->device, &frame, &pass, dd_pass_info);
	renderer_debug_draw_pass(renderer, &frame, &pass);
	end_render_pass(renderer->device, &pass);
	
	struct VulkanBeginPassInfo ui_pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_UI, {renderer->output_rt}, 1};
	begin_render_pass(renderer->device, &frame, &pass, ui_pass_info);
	renderer_imgui_pass(renderer, &frame, &pass);
	end_render_pass(renderer->device, &pass);

	end_frame(renderer->device, &frame, renderer->output_rt);

	TracyCZoneEnd(f);
}
