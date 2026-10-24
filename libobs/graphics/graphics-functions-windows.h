/* libobs/graphics' Windows entry points (obs-rust/obs-studio#872). Included inside
 * graphics.c after its helpers; guarded by its own OS, no include guard. */

#ifdef _WIN32
bool gs_gdi_texture_available(void)
{
	if (!gs_valid("gs_gdi_texture_available"))
		return false;

	return thread_graphics->exports.device_gdi_texture_available();
}

bool gs_shared_texture_available(void)
{
	if (!gs_valid("gs_shared_texture_available"))
		return false;

	return thread_graphics->exports.device_shared_texture_available();
}

bool gs_get_duplicator_monitor_info(int monitor_idx, struct gs_monitor_info *monitor_info)
{
	if (!gs_valid_p("gs_get_duplicator_monitor_info", monitor_info))
		return false;
	if (!thread_graphics->exports.device_get_duplicator_monitor_info)
		return false;

	return thread_graphics->exports.device_get_duplicator_monitor_info(thread_graphics->device, monitor_idx,
									   monitor_info);
}

int gs_duplicator_get_monitor_index(void *monitor)
{
	if (!gs_valid("gs_duplicator_get_monitor_index"))
		return false;
	if (!thread_graphics->exports.device_duplicator_get_monitor_index)
		return false;

	return thread_graphics->exports.device_duplicator_get_monitor_index(thread_graphics->device, monitor);
}

gs_duplicator_t *gs_duplicator_create(int monitor_idx)
{
	if (!gs_valid("gs_duplicator_create"))
		return NULL;
	if (!thread_graphics->exports.device_duplicator_create)
		return NULL;

	return thread_graphics->exports.device_duplicator_create(thread_graphics->device, monitor_idx);
}

void gs_duplicator_destroy(gs_duplicator_t *duplicator)
{
	if (!gs_valid("gs_duplicator_destroy"))
		return;
	if (!duplicator)
		return;
	if (!thread_graphics->exports.gs_duplicator_destroy)
		return;

	thread_graphics->exports.gs_duplicator_destroy(duplicator);
}

bool gs_duplicator_update_frame(gs_duplicator_t *duplicator)
{
	if (!gs_valid_p("gs_duplicator_update_frame", duplicator))
		return false;
	if (!thread_graphics->exports.gs_duplicator_update_frame)
		return false;

	return thread_graphics->exports.gs_duplicator_update_frame(duplicator);
}

bool gs_can_adapter_fast_clear(void)
{
	if (!gs_valid("gs_can_adapter_fast_clear"))
		return false;
	if (!thread_graphics->exports.device_can_adapter_fast_clear)
		return false;

	return thread_graphics->exports.device_can_adapter_fast_clear(thread_graphics->device);
}

gs_texture_t *gs_duplicator_get_texture(gs_duplicator_t *duplicator)
{
	if (!gs_valid_p("gs_duplicator_get_texture", duplicator))
		return NULL;
	if (!thread_graphics->exports.gs_duplicator_get_texture)
		return NULL;

	return thread_graphics->exports.gs_duplicator_get_texture(duplicator);
}

enum gs_color_space gs_duplicator_get_color_space(gs_duplicator_t *duplicator)
{
	if (!gs_valid_p("gs_duplicator_get_color_space", duplicator))
		return GS_CS_SRGB;
	if (!thread_graphics->exports.gs_duplicator_get_color_space)
		return GS_CS_SRGB;

	return thread_graphics->exports.gs_duplicator_get_color_space(duplicator);
}

float gs_duplicator_get_sdr_white_level(gs_duplicator_t *duplicator)
{
	if (!gs_valid_p("gs_duplicator_get_sdr_white_level", duplicator))
		return 80.f;
	if (!thread_graphics->exports.gs_duplicator_get_sdr_white_level)
		return 80.f;

	return thread_graphics->exports.gs_duplicator_get_sdr_white_level(duplicator);
}

/** creates a windows GDI-lockable texture */
gs_texture_t *gs_texture_create_gdi(uint32_t width, uint32_t height)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_texture_create_gdi"))
		return NULL;

	if (graphics->exports.device_texture_create_gdi)
		return graphics->exports.device_texture_create_gdi(graphics->device, width, height);
	return NULL;
}

void *gs_texture_get_dc(gs_texture_t *gdi_tex)
{
	if (!gs_valid_p("gs_texture_release_dc", gdi_tex))
		return NULL;

	if (thread_graphics->exports.gs_texture_get_dc)
		return thread_graphics->exports.gs_texture_get_dc(gdi_tex);
	return NULL;
}

void gs_texture_release_dc(gs_texture_t *gdi_tex)
{
	if (!gs_valid_p("gs_texture_release_dc", gdi_tex))
		return;

	if (thread_graphics->exports.gs_texture_release_dc)
		thread_graphics->exports.gs_texture_release_dc(gdi_tex);
}

gs_texture_t *gs_texture_open_shared(uint32_t handle)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_open_shared"))
		return NULL;

	if (graphics->exports.device_texture_open_shared)
		return graphics->exports.device_texture_open_shared(graphics->device, handle);
	return NULL;
}

gs_texture_t *gs_texture_open_nt_shared(uint32_t handle)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_open_nt_shared"))
		return NULL;

	if (graphics->exports.device_texture_open_nt_shared)
		return graphics->exports.device_texture_open_nt_shared(graphics->device, handle);
	return NULL;
}

uint32_t gs_texture_get_shared_handle(gs_texture_t *tex)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_get_shared_handle"))
		return GS_INVALID_HANDLE;

	if (graphics->exports.device_texture_get_shared_handle)
		return graphics->exports.device_texture_get_shared_handle(tex);
	return GS_INVALID_HANDLE;
}

gs_texture_t *gs_texture_wrap_obj(void *obj)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_wrap_obj"))
		return NULL;

	if (graphics->exports.device_texture_wrap_obj)
		return graphics->exports.device_texture_wrap_obj(graphics->device, obj);
	return NULL;
}

int gs_texture_acquire_sync(gs_texture_t *tex, uint64_t key, uint32_t ms)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_acquire_sync"))
		return -1;

	if (graphics->exports.device_texture_acquire_sync)
		return graphics->exports.device_texture_acquire_sync(tex, key, ms);
	return -1;
}

int gs_texture_release_sync(gs_texture_t *tex, uint64_t key)
{
	graphics_t *graphics = thread_graphics;
	if (!gs_valid("gs_texture_release_sync"))
		return -1;

	if (graphics->exports.device_texture_release_sync)
		return graphics->exports.device_texture_release_sync(tex, key);
	return -1;
}

gs_stagesurf_t *gs_stagesurface_create_nv12(uint32_t width, uint32_t height)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stagesurface_create_nv12"))
		return NULL;

	if ((width & 1) == 1 || (height & 1) == 1) {
		blog(LOG_ERROR, "NV12 textures must have dimensions "
				"divisible by 2.");
		return NULL;
	}

	if (graphics->exports.device_stagesurface_create_nv12)
		return graphics->exports.device_stagesurface_create_nv12(graphics->device, width, height);

	return NULL;
}

gs_stagesurf_t *gs_stagesurface_create_p010(uint32_t width, uint32_t height)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_stagesurface_create_p010"))
		return NULL;

	if ((width & 1) == 1 || (height & 1) == 1) {
		blog(LOG_ERROR, "P010 textures must have dimensions "
				"divisible by 2.");
		return NULL;
	}

	if (graphics->exports.device_stagesurface_create_p010)
		return graphics->exports.device_stagesurface_create_p010(graphics->device, width, height);

	return NULL;
}

void gs_register_loss_callbacks(const struct gs_device_loss *callbacks)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_register_loss_callbacks"))
		return;

	if (graphics->exports.device_register_loss_callbacks)
		graphics->exports.device_register_loss_callbacks(graphics->device, callbacks);
}

void gs_unregister_loss_callbacks(void *data)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid("gs_unregister_loss_callbacks"))
		return;

	if (graphics->exports.device_unregister_loss_callbacks)
		graphics->exports.device_unregister_loss_callbacks(graphics->device, data);
}

#endif
