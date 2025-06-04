#pragma once

typedef struct VulkanDevice VulkanDevice;
typedef struct VulkanFrame VulkanFrame;
typedef struct VulkanRenderPass VulkanRenderPass;

enum ImageFormat
{
	PG_FORMAT_NONE = 0,
	PG_FORMAT_R8_UNORM = 9,
	PG_FORMAT_R8G8B8A8_UNORM = 37,
	PG_FORMAT_D32_SFLOAT = 126,
	PG_FORMAT_B10G11R11_UFLOAT_PACK32 = 122,
	PG_FORMAT_RGBA16F = 97,
};

struct RenderPass
{
	const char* name;
	enum ImageFormat color_formats[8];
	uint32_t color_formats_length;
	enum ImageFormat depth_format;
	bool depth_enable_test;
	bool depth_enable_write;
};

enum RenderPassesId
{
	RENDER_PASSES_MESH = 0,
	RENDER_PASSES_DEBUG_DRAW,
	RENDER_PASSES_UI,
	RENDER_PASSES_COMPOSITING,
	RENDER_PASSES_COUNT,
};

static struct RenderPass RENDER_PASSES[RENDER_PASSES_COUNT] =
{
	{"mesh", {PG_FORMAT_RGBA16F}, 1, PG_FORMAT_D32_SFLOAT, true, true},
	{"debugdraw", {PG_FORMAT_R8G8B8A8_UNORM}, 1, PG_FORMAT_NONE, false, false},
	{"ui", {PG_FORMAT_R8G8B8A8_UNORM}, 1, PG_FORMAT_NONE, false, false},
	{"compositing", {PG_FORMAT_R8G8B8A8_UNORM}, 1, PG_FORMAT_NONE, false, false},
};

enum VulkanTopology
{
	VULKAN_TOPOLOGY_POINT_LIST = 0,
	VULKAN_TOPOLOGY_LINE_LIST = 1,
	VULKAN_TOPOLOGY_TRIANGLE_LIST = 3,
};
enum VulkanFillMode
{
	VULKAN_FILL_MODE_FILL = 0,
	VULKAN_FILL_MODE_LINE = 1,
	VULKAN_FILL_MODE_POINT = 1,
};
struct VulkanGraphicsPsoSpec
{
	enum VulkanTopology topology;
	enum VulkanFillMode fillmode;
};

struct VulkanBeginPassInfo
{
	uint32_t pass_id;
	uint32_t color_rts[8];
	uint32_t color_rts_length;
	uint32_t depth_rt;
};

struct VulkanScissor
{
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct VulkanDraw
{
    uint32_t    index_count;
    uint32_t    instance_count;
    uint32_t    first_index;
    int32_t     vertex_offset;
    uint32_t    first_instance;
};

union VulkanClearColor
{
	float f32[4];
	uint32_t u32[4];
};

uint32_t vulkan_get_device_size(void);
void vulkan_create_device(VulkanDevice *device, void *hwnd);

void new_graphics_program(VulkanDevice *device, uint32_t handle, MaterialAsset material_asset);
void new_graphics_program_ex(VulkanDevice *device, uint32_t handle, MaterialAsset material_asset, struct VulkanGraphicsPsoSpec spec);

void new_compute_program(VulkanDevice *device, uint32_t handle, ComputeProgramAsset program_asset);

void new_index_buffer(VulkanDevice *device, uint32_t handle, uint32_t size);
void new_storage_buffer(VulkanDevice *device, uint32_t handle, uint32_t size);
void* buffer_get_mapped_pointer(VulkanDevice *device, uint32_t handle);
uint64_t buffer_get_gpu_address(VulkanDevice *device, uint32_t handle);
uint32_t buffer_get_size(VulkanDevice *device, uint32_t handle);

void new_render_target(VulkanDevice *device, uint32_t handle, uint32_t width, uint32_t height, int format);
void resize_render_target(VulkanDevice *device, uint32_t handle, uint32_t width, uint32_t height);

void new_texture(VulkanDevice *device, uint32_t handle, uint32_t width, uint32_t height, int format, void *data, uint32_t size);

void begin_frame(VulkanDevice *device, VulkanFrame *frame, uint32_t *out_swapchain_w, uint32_t *out_swapchain_h);
void end_frame(VulkanDevice *device, VulkanFrame *fame, uint32_t output_rt);
void begin_render_pass(VulkanDevice *device, VulkanFrame *frame, VulkanRenderPass *pass, struct VulkanBeginPassInfo pass_info);
void begin_render_pass_discard(VulkanDevice *device, VulkanFrame *frame, VulkanRenderPass *pass, struct VulkanBeginPassInfo pass_info);

void end_render_pass(VulkanDevice *device, VulkanRenderPass *pass);

void vulkan_clear(VulkanDevice *device, VulkanRenderPass *pass, union VulkanClearColor const* colors, uint32_t colors_length, float depth);
void vulkan_set_scissor(VulkanDevice *device, VulkanRenderPass *pass, struct VulkanScissor scissor);
void vulkan_push_constants(VulkanDevice *Device, VulkanRenderPass *pass, void *data, uint32_t size);
void vulkan_bind_graphics_pso(VulkanDevice *device, VulkanRenderPass *pass, uint32_t pso);
void vulkan_bind_index_buffer(VulkanDevice *device, VulkanRenderPass *pass, uint32_t index_buffer);
void vulkan_draw(VulkanDevice *device, VulkanRenderPass *pass, struct VulkanDraw draw);
void vulkan_draw_not_indexed(VulkanDevice *device, VulkanRenderPass *pass, uint32_t vertex_count);
void vulkan_insert_debug_label(VulkanDevice *device, VulkanRenderPass *pass, const char *label);

// temp
void vulkan_bind_texture(VulkanDevice *device, VulkanFrame *frame, uint32_t texture, uint32_t slot);
