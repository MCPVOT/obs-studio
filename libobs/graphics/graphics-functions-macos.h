/* libobs/graphics' macOS entry points (obs-rust/obs-studio#872). Included inside
 * graphics.c after its helpers; guarded by its own OS, no include guard. */

#ifdef __APPLE__
/** Platform specific functions */
gs_texture_t *gs_texture_create_from_iosurface(void *iosurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_create_from_iosurface", iosurf))
		return NULL;
	if (!graphics->exports.device_texture_create_from_iosurface)
		return NULL;

	return graphics->exports.device_texture_create_from_iosurface(graphics->device, iosurf);
}

bool gs_texture_rebind_iosurface(gs_texture_t *texture, void *iosurf)
{
	graphics_t *graphics = thread_graphics;

	if (!gs_valid_p("gs_texture_rebind_iosurface", texture))
		return false;
	if (!graphics->exports.gs_texture_rebind_iosurface)
		return false;

	return graphics->exports.gs_texture_rebind_iosurface(texture, iosurf);
}

bool gs_shared_texture_available(void)
{
	if (!gs_valid("gs_shared_texture_available"))
		return false;

	return thread_graphics->exports.device_shared_texture_available();
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

#endif
