#include "renderer.h"
#include "file.h"
#include "drawer2d.h"

#define RENDERER_SKELETAL_MESH_CAPACITY (8)
#define RENDERER_MESH_VERTEX_CAPACITY (64 << 10)
#define RENDERER_MESH_INDEX_CAPACITY (64 << 10)
#define RENDERER_UPLOAD_BUFFER_SIZE (64 << 20)

/**

   The renderer orchestrates everything using the RHI api.
   Its job is to:
   - create all pipeline resources (buffers, textures, PSOs)
   - describe render pipeline with immediate mode RHI api (Render targets, Render passes, binding PSOs, submitting draw calls and dispatches).

   ==3D object pipeline==
   geometry buffer: position buffer + normal buffer
   skinned geometry buffer: position buffer + normal buffer + bone indices/weight buffer
   mesh buffer: (first index, index count, first_vertex) buffer
   instance buffer:  (mesh, transform) buffer


   Static Mesh: vertices + indices in global geometry buffers
   Skeletal Mesh: vertices + indices in global deformable geometry buffers + global geometry buffers
   Instance: Mesh + dynamic transform
   SkinnedInstance: Skeletal Mesh + bone matrices + dynamic transform

   1st: skinned geometry dispatch, read bone matrices and output final vertex positions in global geometry buffer
   2nd: depth prepass + motion vector (position only vertex shader + motion vector pixel shader)
   3rd: forward pass (everything vertex shader + forward pixel shader)
 **/

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
	Float3x4 invview;
	Float3x4 transform;
	uint64_t ibl_buffer;
	uint64_t bones_matrices_buffer;
	uint64_t positions_vbuffer;
	uint64_t normals_vbuffer;
};

struct RenderMesh
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
	// render targets
	uint32_t hdr_rt;
	uint32_t depth_rt;
	uint32_t output_rt;
	uint32_t final_rt;
	// drawer2d
	uint32_t drawer2d_pso;
	uint32_t drawer2d_ibuffer;
	uint32_t drawer2d_vbuffer;
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
	uint32_t mesh_depth_pso;
	uint32_t mesh_ibuffer;
	oa_allocator_t mesh_vbuffer_allocator;
	oa_allocator_t mesh_ibuffer_allocator;
	uint32_t mesh_skinned_positions_vbuffer;
	uint32_t mesh_skinned_normals_vbuffer;
	uint32_t mesh_skinned_bone_indices_weights_vbuffer;
	uint32_t mesh_positions_vbuffer;
	uint32_t mesh_normals_vbuffer;
	struct RenderMesh meshes[RENDERER_SKELETAL_MESH_CAPACITY];
	// background shaders
	uint32_t bg0_pso;
	// postfx shaders
	uint32_t compositing_pso;
	// IBL
	uint32_t convolve_diffuse_irradiance_pso;
	uint32_t diffuse_ibl_buffer;
	// context
	Float4x4 proj;
	Float4x4 invproj;
	Float3x4 view;
	Float3x4 invview;
	float time;
	uint32_t constant_buffer;
	uint32_t constant_offset;
	uint32_t upload_buffer;
	uint32_t upload_offset;
	bool is_hdr;
	// scene
	struct Camera main_camera;
	struct SkeletalMeshInstanceData skeletal_mesh_instances[2];
	uint32_t skeletal_mesh_instances_length;
	struct Drawer2D *drawer;
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
			  PG_FORMAT_R8G8B8A8_UNORM,
			  1);

	renderer->hdr_rt = 1;
	new_render_target(renderer->device, renderer->hdr_rt,
			  renderer->device->swapchain_width / 2,
			  renderer->device->swapchain_height / 2,
			  PG_FORMAT_RGBA16F,
			  4);

	renderer->depth_rt = 2;
	new_render_target(renderer->device, renderer->depth_rt,
			  renderer->device->swapchain_width / 2,
			  renderer->device->swapchain_height / 2,
			  PG_FORMAT_D32_SFLOAT,
			  4);

	enum ImageFormat surface_format = vulkan_get_surface_format(renderer->device);
	renderer->is_hdr = (surface_format == PG_FORMAT_A2B10G10R10_UNORM_PACK32);

	renderer->final_rt = 3;
	new_render_target(renderer->device, renderer->final_rt,
			  renderer->device->swapchain_width,
			  renderer->device->swapchain_height,
			  surface_format,
			  1);

	// Create drawer2d resources
	renderer->drawer2d_ibuffer = 0;
	renderer->drawer2d_vbuffer = 1;
	new_index_buffer(renderer->device, renderer->drawer2d_ibuffer, DRAWER_2D_INDEX_CAPACITY * sizeof(uint32_t));
	new_storage_buffer(renderer->device, renderer->drawer2d_vbuffer, DRAWER_2D_VERTEX_CAPACITY * sizeof(struct Vertex2D));

	// Create imgui resources
	renderer->imgui_fontatlas = 0;
	renderer->imgui_ibuffer = 2;
	renderer->imgui_vbuffer = 3;
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
	renderer->dd_ibuffer = 4;
	renderer->dd_vbuffer = 5;
	new_index_buffer(renderer->device, renderer->dd_ibuffer, (64 << 10));
	new_storage_buffer(renderer->device, renderer->dd_vbuffer, (64 << 10));

	// Create mesh resources
	uint32_t MAX_MESH_ALLOCATIONS = 32;
	int res = oa_create(&renderer->mesh_vbuffer_allocator, RENDERER_MESH_VERTEX_CAPACITY, MAX_MESH_ALLOCATIONS);
	assert(res == 0);
	res = oa_create(&renderer->mesh_ibuffer_allocator, RENDERER_MESH_INDEX_CAPACITY, MAX_MESH_ALLOCATIONS);
	assert(res == 0);
	renderer->mesh_ibuffer = 6;
	renderer->mesh_skinned_positions_vbuffer = 7;
	renderer->mesh_skinned_normals_vbuffer = 8;
	renderer->mesh_skinned_bone_indices_weights_vbuffer = 9;
	renderer->mesh_positions_vbuffer = 10;
	renderer->mesh_normals_vbuffer = 11;

	new_index_buffer(renderer->device, renderer->mesh_ibuffer, RENDERER_MESH_INDEX_CAPACITY * sizeof(uint32_t));
	new_storage_buffer(renderer->device, renderer->mesh_skinned_positions_vbuffer, RENDERER_MESH_VERTEX_CAPACITY * sizeof(Float3));
	new_storage_buffer(renderer->device, renderer->mesh_skinned_normals_vbuffer, RENDERER_MESH_VERTEX_CAPACITY * sizeof(Float3));
	new_storage_buffer(renderer->device, renderer->mesh_skinned_bone_indices_weights_vbuffer, RENDERER_MESH_VERTEX_CAPACITY * 2 * sizeof(uint32_t));
	new_storage_buffer(renderer->device, renderer->mesh_positions_vbuffer, RENDERER_MESH_VERTEX_CAPACITY * sizeof(Float3));
	new_storage_buffer(renderer->device, renderer->mesh_normals_vbuffer, RENDERER_MESH_VERTEX_CAPACITY * sizeof(Float3));

	// memset(buffer_get_mapped_pointer(renderer->device, renderer->mesh_vbuffer), 0, buffer_get_size(renderer->device, renderer->mesh_vbuffer));
	// memset(buffer_get_mapped_pointer(renderer->device, renderer->mesh_ibuffer), 0, buffer_get_size(renderer->device, renderer->mesh_ibuffer));

	renderer->constant_buffer = 12;
	new_storage_buffer(renderer->device, renderer->constant_buffer, (64 << 10));

	renderer->upload_buffer = 13;
	new_upload_buffer(renderer->device, renderer->upload_buffer, RENDERER_UPLOAD_BUFFER_SIZE);

	renderer->diffuse_ibl_buffer = 14;
	new_storage_buffer(renderer->device, renderer->diffuse_ibl_buffer, (64 << 10));

	renderer_init_materials(renderer, assets);

	renderer->main_camera.position = (Float3){1.0f, -5.0f, 1.0f};
	renderer->main_camera.vertical_fov = 40.0f;
}

void renderer_init_materials(Renderer *renderer, struct AssetLibrary *assets)
{
	struct MaterialAsset const *drawer2d_material = asset_library_get_material(assets, 3339345721);
	struct MaterialAsset const *imgui_material = asset_library_get_material(assets, 3661877039);
	struct MaterialAsset const *dd_material = asset_library_get_material(assets, 3200094153);
	struct MaterialAsset const *mesh_material = asset_library_get_material(assets, 2455425701);
	struct MaterialAsset const *bg0_material = asset_library_get_material(assets, 295627051);
	struct ComputeProgramAsset const *convolve_diffuse_irradiance_program = asset_library_get_compute_program(assets, 3356797155);
	struct ComputeProgramAsset const *compositing_program = asset_library_get_compute_program(assets, 4212483871);

	struct MaterialAsset mesh_depth_material = *mesh_material;
	mesh_depth_material.pixel_shader_bytecode.data = NULL;
	mesh_depth_material.pixel_shader_bytecode.size = 0;

	renderer->imgui_pso = 0;
	new_graphics_program(renderer->device, renderer->imgui_pso, *imgui_material);

	renderer->drawer2d_pso = 7;
	new_graphics_program(renderer->device, renderer->drawer2d_pso, *drawer2d_material);

	renderer->dd_pso = 1;
	renderer->dd_line_pso = 2;
	struct VulkanGraphicsPsoSpec pso_spec = {0};
	pso_spec.topology = VULKAN_TOPOLOGY_TRIANGLE_LIST;
	pso_spec.fillmode = VULKAN_FILL_MODE_LINE;
	new_graphics_program_ex(renderer->device, renderer->dd_pso, *dd_material, pso_spec);
	pso_spec.topology = VULKAN_TOPOLOGY_LINE_LIST;
	new_graphics_program_ex(renderer->device, renderer->dd_line_pso, *dd_material, pso_spec);

	renderer->mesh_pso = 3;
	new_graphics_program(renderer->device, renderer->mesh_pso, *mesh_material);
	renderer->mesh_depth_pso = 6;
	new_graphics_program(renderer->device, renderer->mesh_depth_pso, mesh_depth_material);

	renderer->bg0_pso = 4;
	new_graphics_program(renderer->device, renderer->bg0_pso, *bg0_material);

	renderer->compositing_pso = 0;
	new_compute_program(renderer->device, renderer->compositing_pso, *compositing_program);

	renderer->convolve_diffuse_irradiance_pso = 1;
	new_compute_program(renderer->device, renderer->convolve_diffuse_irradiance_pso, *convolve_diffuse_irradiance_program);
}

void renderer_create_render_skeletal_mesh(Renderer *renderer, struct SkeletalMeshAsset *asset, uint32_t handle)
{
	oa_allocation_t vbuffer_allocation = {0};
	int alloc_res = oa_allocate(&renderer->mesh_vbuffer_allocator, asset->vertices_length, &vbuffer_allocation);
	assert(alloc_res == 0);
	oa_allocation_t ibuffer_allocation = {0};
	alloc_res = oa_allocate(&renderer->mesh_ibuffer_allocator, asset->indices_length, &ibuffer_allocation);
	assert(alloc_res == 0);

	renderer->meshes[handle].vbuffer_allocation = vbuffer_allocation;
	renderer->meshes[handle].ibuffer_allocation = ibuffer_allocation;
	renderer->meshes[handle].index_count = asset->indices_length;
	renderer->meshes[handle].first_index = ibuffer_allocation.offset;
	renderer->meshes[handle].vertex_offset = vbuffer_allocation.offset;

	uint32_t *gpu_indices = buffer_get_mapped_pointer(renderer->device, renderer->mesh_ibuffer);
	Float3 *gpu_positions = buffer_get_mapped_pointer(renderer->device, renderer->mesh_positions_vbuffer);
	Float3 *gpu_normals = buffer_get_mapped_pointer(renderer->device, renderer->mesh_normals_vbuffer);

	memcpy(gpu_indices + ibuffer_allocation.offset, asset->indices, asset->indices_length * sizeof(uint32_t));
	memcpy(gpu_positions + vbuffer_allocation.offset, asset->vertices_positions, asset->vertices_length * sizeof(Float3));
	memcpy(gpu_normals + vbuffer_allocation.offset, asset->vertices_normals, asset->vertices_length * sizeof(Float3));

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

void* renderer_temp_allocate_gpu(Renderer *renderer, uint32_t size)
{
	if (renderer->upload_offset + size > RENDERER_UPLOAD_BUFFER_SIZE) {
		return NULL;
	}

	char *data = buffer_get_mapped_pointer(renderer->device, renderer->upload_buffer);
	uint32_t offset = renderer->upload_offset;
	renderer->upload_offset += size;
	return data + offset;
}

void renderer_upload_texture(Renderer* renderer, struct RendererTextureUpload upload)
{
	char *upload_begin = buffer_get_mapped_pointer(renderer->device, renderer->upload_buffer);
	char *upload_end = upload_begin + RENDERER_UPLOAD_BUFFER_SIZE;
	assert(upload_begin <= upload.temp_data && upload.temp_data < upload_end);
	uint32_t buffer_offset = (uint32_t)((uint64_t)((char*)upload.temp_data - upload_begin));

	assert(upload.width > 0);
	assert(upload.height > 0);

	vulkan_copy_buffer_to_texture(renderer->device, (struct VulkanBufferTextureCopy){
			.buffer = renderer->upload_buffer,
			.texture = upload.texture,
			.offset = buffer_offset,
			.width  = upload.width,
			.height = upload.height,
			.x_offset = upload.x_offset,
			.y_offset = upload.y_offset,
		});
}


static void renderer_debug_draw_pass(Renderer *renderer, VulkanFrame *frame, VulkanRenderPass *pass)
{
	uint32_t vertex_size = g_dd.points_length * 6 * sizeof(struct DdVert); //  6 vertices
	uint32_t index_size = g_dd.points_length * 8 * 3 * sizeof(uint32_t); // 2 faces

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

	vulkan_push_constants(renderer->device, pass->frame, &constants, sizeof(constants));
	vulkan_bind_index_buffer(renderer->device, pass, renderer->dd_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, pass, renderer->dd_pso);

	vulkan_insert_debug_label(renderer->device, pass->frame, "dd: spheres");
	vulkan_draw(renderer->device, pass, spheres_draw);

	vulkan_bind_graphics_pso(renderer->device, pass, renderer->dd_line_pso);

	vulkan_insert_debug_label(renderer->device, pass->frame, "dd: lines");
	vulkan_draw(renderer->device, pass, lines_draw);
}

static void renderer_drawer2d_pass(Renderer *renderer, VulkanFrame *frame, VulkanRenderPass *pass)
{
	struct Drawer2D *drawer = renderer->drawer;
	float display_width = drawer->viewport_width;
	float display_height = drawer->viewport_height;

	// Upload vertices
	uint32_t vbuffer_size = buffer_get_size(renderer->device, renderer->drawer2d_vbuffer);
	uint32_t ibuffer_size = buffer_get_size(renderer->device, renderer->drawer2d_ibuffer);
	assert(vbuffer_size == sizeof(drawer->current_vertices));
	assert(ibuffer_size == sizeof(drawer->current_indices));

	struct Vertex2D *vertices_gpu = buffer_get_mapped_pointer(renderer->device, renderer->drawer2d_vbuffer);
	uint32_t *indices_gpu = buffer_get_mapped_pointer(renderer->device, renderer->drawer2d_ibuffer);

	memcpy(vertices_gpu, drawer->current_vertices, sizeof(drawer->current_vertices));
	memcpy(indices_gpu, drawer->current_indices, sizeof(drawer->current_indices));

	// Render
	struct Drawer2DPushConstants
	{
		float scale[2];
		float translation[2];
		uint64_t vbuffer;
	} constants;
	constants.scale[0] = 2.0f / display_width;
	constants.scale[1] = 2.0f / display_height;
	constants.translation[0] = -1.0f;
	constants.translation[1] = -1.0f;
	constants.vbuffer = buffer_get_gpu_address(renderer->device, renderer->drawer2d_vbuffer);

	vulkan_push_constants(renderer->device, pass->frame, &constants, sizeof(constants));
	vulkan_bind_index_buffer(renderer->device, pass, renderer->drawer2d_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, pass, renderer->drawer2d_pso);
	vulkan_insert_debug_label(renderer->device, pass->frame, "drawer2d");

	// Apply scissor/clipping rectangle
	struct VulkanScissor scissor = {};
	scissor.x = 0;
	scissor.y = 0;
	scissor.w = display_width;
	scissor.h = display_height;
	vulkan_set_scissor(renderer->device, pass, scissor);

	struct VulkanDraw draw = {drawer->current_indices_length, 1, 0, 0, 0};
	vulkan_draw(renderer->device, pass, draw);
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
	vulkan_push_constants(renderer->device, pass->frame, &constants, sizeof(constants));
	vulkan_bind_index_buffer(renderer->device, pass, renderer->imgui_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, pass, renderer->imgui_pso);
	vulkan_insert_debug_label(renderer->device, pass->frame, "imgui");

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

void renderer_set_main_camera(Renderer *renderer, struct Camera camera)
{
	renderer->main_camera = camera;
}

void renderer_set_time(Renderer *renderer, float t)
{
	renderer->time = t;
}

void renderer_set_drawer2d(Renderer *renderer, struct Drawer2D *drawer)
{
	renderer->drawer = drawer;
}


void renderer_render(Renderer *renderer)
{
	TracyCZoneN(f, "Renderer render", true);

	VulkanFrame frame = {0};

	uint32_t swapchain_width = 0;
	uint32_t swapchain_height = 0;
	begin_frame(renderer->device, &frame, &swapchain_width, &swapchain_height);

	renderer->proj = perspective_projection(renderer->main_camera.vertical_fov, (float)swapchain_width / (float)swapchain_height, 1.0f, 100.0f, &renderer->invproj);
	renderer->view = lookat_view(renderer->main_camera.position, renderer->main_camera.lookat, &renderer->invview);

	if (renderer->device->rts[renderer->output_rt].width != swapchain_width || renderer->device->rts[renderer->output_rt].height != swapchain_height) {
		resize_render_target(renderer->device, renderer->output_rt, swapchain_width, swapchain_height);
		resize_render_target(renderer->device, renderer->hdr_rt, swapchain_width / 2, swapchain_height / 2);
		resize_render_target(renderer->device, renderer->depth_rt, swapchain_width / 2, swapchain_height / 2);
		resize_render_target(renderer->device, renderer->final_rt, swapchain_width, swapchain_height);
	}
	vulkan_bind_texture(renderer->device, &frame, renderer->imgui_fontatlas, 0);
	vulkan_bind_texture(renderer->device, &frame, renderer->drawer->glyph_cache_texture, 4);

	{
		struct ConvolveConstants
		{
			uint64_t ibl_buffer;
		} constants;
		constants.ibl_buffer = buffer_get_gpu_address(renderer->device, renderer->diffuse_ibl_buffer);
		vulkan_insert_debug_label(renderer->device, &frame, "IBL: diffuse");
		vulkan_bind_compute_pso(renderer->device, &frame, renderer->convolve_diffuse_irradiance_pso);
		vulkan_push_constants(renderer->device, &frame, &constants, sizeof(constants));
		vulkan_dispatch(renderer->device, &frame, 1, 1, 1);
	}

	VulkanRenderPass pass = {0};
	union VulkanClearColor clear_color = {0};

	struct VulkanBeginPassInfo mesh_pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_MESH, {renderer->hdr_rt}, 1, renderer->depth_rt};
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
			uint64_t ibl_buffer;
		} constants;
		constants.proj = renderer->proj;
		constants.invproj = renderer->invproj;
		constants.view = renderer->view;
		constants.invview = renderer->invview;
		constants.resolution = (Float3){swapchain_width, swapchain_height, 1};
		constants.time = renderer->time;
		constants.ibl_buffer = buffer_get_gpu_address(renderer->device, renderer->diffuse_ibl_buffer);

		vulkan_insert_debug_label(renderer->device, pass.frame, "background");
		vulkan_bind_graphics_pso(renderer->device, &pass, renderer->bg0_pso);
		vulkan_push_constants(renderer->device, pass.frame, &constants, sizeof(constants));
		vulkan_draw_not_indexed(renderer->device, &pass, 3);
	}

	struct SkeletalMeshConstants mesh_constants[8] = {};
	// prepare mesh constants
	for (uint32_t iinstance = 0; iinstance < renderer->skeletal_mesh_instances_length && iinstance < ARRAY_LENGTH(mesh_constants); ++iinstance){
		struct RenderMesh *render_mesh = &renderer->meshes[renderer->skeletal_mesh_instances[iinstance].mesh_render_handle];
		struct SkeletalMeshInstance const *dynamic_data_mesh = renderer->skeletal_mesh_instances[iinstance].dynamic_data_mesh;
		Float3x4 const* dynamic_data_transform = &renderer->skeletal_mesh_instances[iinstance].dynamic_data_spatial->world_transform;
		// upload bone matrices
		// char *gpu_data = buffer_get_mapped_pointer(renderer->device, renderer->constant_buffer);
		// gpu_data += renderer->constant_offset;
		// assert(renderer->constant_offset + sizeof(struct SkeletalMeshCBuffer) <= buffer_get_size(renderer->device, renderer->constant_buffer));
		// struct SkeletalMeshCBuffer *gpu_cbuffer = (struct SkeletalMeshCBuffer*)gpu_data;
		// memcpy(gpu_cbuffer, dynamic_data_mesh->pose, sizeof(dynamic_data_mesh->pose));

		// prepare constants
		struct SkeletalMeshConstants constants = {0};
		constants.proj = renderer->proj;
		constants.view = renderer->view;
		constants.invview = renderer->invview;
		constants.transform = *dynamic_data_transform;
		constants.ibl_buffer = buffer_get_gpu_address(renderer->device, renderer->diffuse_ibl_buffer);
		constants.bones_matrices_buffer = buffer_get_gpu_address(renderer->device, renderer->constant_buffer) + renderer->constant_offset;
		constants.positions_vbuffer = buffer_get_gpu_address(renderer->device, renderer->mesh_positions_vbuffer);
		constants.normals_vbuffer = buffer_get_gpu_address(renderer->device, renderer->mesh_normals_vbuffer);
		mesh_constants[iinstance] = constants;
		renderer->constant_offset += sizeof(struct SkeletalMeshCBuffer);
	}
	renderer->constant_offset = 0;

	vulkan_insert_debug_label(renderer->device, pass.frame, "skeletal meshes depth");
	vulkan_bind_index_buffer(renderer->device, &pass, renderer->mesh_ibuffer);
	vulkan_bind_graphics_pso(renderer->device, &pass, renderer->mesh_depth_pso);
	for (uint32_t iinstance = 0; iinstance < renderer->skeletal_mesh_instances_length && iinstance < ARRAY_LENGTH(mesh_constants); ++iinstance){
		struct RenderMesh *render_mesh = &renderer->meshes[renderer->skeletal_mesh_instances[iinstance].mesh_render_handle];
		vulkan_push_constants(renderer->device, pass.frame, &mesh_constants[iinstance], sizeof(struct SkeletalMeshConstants));
		struct VulkanDraw draw = {render_mesh->index_count,
					  1,
					  render_mesh->first_index,
					  render_mesh->vertex_offset,
					  0};
		vulkan_draw(renderer->device, &pass, draw);

	}
	vulkan_insert_debug_label(renderer->device, pass.frame, "skeletal meshes shading");
	vulkan_bind_graphics_pso(renderer->device, &pass, renderer->mesh_pso);
	for (uint32_t iinstance = 0; iinstance < renderer->skeletal_mesh_instances_length; ++iinstance){
		struct RenderMesh *render_mesh = &renderer->meshes[renderer->skeletal_mesh_instances[iinstance].mesh_render_handle];
		vulkan_push_constants(renderer->device, pass.frame, &mesh_constants[iinstance], sizeof(struct SkeletalMeshConstants));
		struct VulkanDraw draw = {render_mesh->index_count,
					  1,
					  render_mesh->first_index,
					  render_mesh->vertex_offset,
					  0};
		vulkan_draw(renderer->device, &pass, draw);
	}
	end_render_pass(renderer->device, &pass);

	struct VulkanBeginPassInfo dd_pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_DEBUG_DRAW, {renderer->output_rt}, 1};
	begin_render_pass_discard(renderer->device, &frame, &pass, dd_pass_info);
	vulkan_clear(renderer->device, &pass, &clear_color, 1, 0.0f);
	renderer_debug_draw_pass(renderer, &frame, &pass);
	end_render_pass(renderer->device, &pass);

	struct VulkanBeginPassInfo ui_pass_info = (struct VulkanBeginPassInfo){RENDER_PASSES_UI, {renderer->output_rt}, 1};
	begin_render_pass(renderer->device, &frame, &pass, ui_pass_info);
	renderer_drawer2d_pass(renderer, &frame, &pass);
	renderer_imgui_pass(renderer, &frame, &pass);
	end_render_pass(renderer->device, &pass);

	vulkan_bind_rt_as_texture(renderer->device, &frame, renderer->hdr_rt, 1);
	vulkan_bind_rt_as_texture(renderer->device, &frame, renderer->output_rt, 2);
	vulkan_bind_rt_as_image(renderer->device, &frame, renderer->final_rt, 0);
	{
		struct CompositingConstants
		{
			int is_hdr;
		} constants;
		constants.is_hdr = renderer->is_hdr;
		vulkan_insert_debug_label(renderer->device, &frame, "compositing");
		vulkan_bind_compute_pso(renderer->device, &frame, renderer->compositing_pso);
		vulkan_push_constants(renderer->device, &frame, &constants, sizeof(constants));
		uint32_t x = (swapchain_width + 15) / 16;
		uint32_t y = (swapchain_height + 15) / 16;
		vulkan_dispatch(renderer->device, &frame, x, y, 1);
	}

	end_frame(renderer->device, &frame, renderer->final_rt);

	// reset per-frame state
	renderer->upload_offset = 0;

	TracyCZoneEnd(f);
}
