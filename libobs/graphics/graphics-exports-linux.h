/* libobs/graphics' Linux and BSD entry points in struct gs_exports (obs-rust/obs-studio#872). Included inside
 * struct gs_exports; no include guard. */

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__)
struct gs_texture *(*device_texture_create_from_dmabuf)(gs_device_t *device, unsigned int width, unsigned int height,
							uint32_t drm_format, enum gs_color_format color_format,
							uint32_t n_planes, const int *fds, const uint32_t *strides,
							const uint32_t *offsets, const uint64_t *modifiers);
bool (*device_query_dmabuf_capabilities)(gs_device_t *device, enum gs_dmabuf_flags *dmabuf_flags,
					 uint32_t **drm_formats, size_t *n_formats);
bool (*device_query_dmabuf_modifiers_for_format)(gs_device_t *device, uint32_t drm_format, uint64_t **modifiers,
						 size_t *n_modifiers);
struct gs_texture *(*device_texture_create_from_pixmap)(gs_device_t *device, uint32_t width, uint32_t height,
							enum gs_color_format color_format, uint32_t target,
							void *pixmap);
bool (*device_query_sync_capabilities)(gs_device_t *device);
gs_sync_t *(*device_sync_create)(gs_device_t *device);
gs_sync_t *(*device_sync_create_from_syncobj_timeline_point)(gs_device_t *device, int syncobj_fd,
							     uint64_t timeline_point);
void (*device_sync_destroy)(gs_device_t *device, gs_sync_t *sync);
bool (*device_sync_export_syncobj_timeline_point)(gs_device_t *device, gs_sync_t *sync, int syncobj_fd,
						  uint64_t timeline_point);
bool (*device_sync_signal_syncobj_timeline_point)(gs_device_t *device, int syncobj_fd, uint64_t timeline_point);
bool (*device_sync_wait)(gs_device_t *device, gs_sync_t *sync);
#endif
