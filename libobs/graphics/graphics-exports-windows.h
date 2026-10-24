/* libobs/graphics' Windows entry points in struct gs_exports (obs-rust/obs-studio#872). Included inside
 * struct gs_exports; no include guard. */

#ifdef _WIN32
bool (*device_gdi_texture_available)(void);
bool (*device_shared_texture_available)(void);

bool (*device_get_duplicator_monitor_info)(gs_device_t *device, int monitor_idx, struct gs_monitor_info *monitor_info);
int (*device_duplicator_get_monitor_index)(gs_device_t *device, void *monitor);

gs_duplicator_t *(*device_duplicator_create)(gs_device_t *device, int monitor_idx);
void (*gs_duplicator_destroy)(gs_duplicator_t *duplicator);

bool (*gs_duplicator_update_frame)(gs_duplicator_t *duplicator);
gs_texture_t *(*gs_duplicator_get_texture)(gs_duplicator_t *duplicator);
enum gs_color_space (*gs_duplicator_get_color_space)(gs_duplicator_t *duplicator);
float (*gs_duplicator_get_sdr_white_level)(gs_duplicator_t *duplicator);

bool (*device_can_adapter_fast_clear)(gs_device_t *device);

gs_texture_t *(*device_texture_create_gdi)(gs_device_t *device, uint32_t width, uint32_t height);

void *(*gs_texture_get_dc)(gs_texture_t *gdi_tex);
void (*gs_texture_release_dc)(gs_texture_t *gdi_tex);

gs_texture_t *(*device_texture_open_shared)(gs_device_t *device, uint32_t handle);
gs_texture_t *(*device_texture_open_nt_shared)(gs_device_t *device, uint32_t handle);
uint32_t (*device_texture_get_shared_handle)(gs_texture_t *tex);
gs_texture_t *(*device_texture_wrap_obj)(gs_device_t *device, void *obj);
int (*device_texture_acquire_sync)(gs_texture_t *tex, uint64_t key, uint32_t ms);
int (*device_texture_release_sync)(gs_texture_t *tex, uint64_t key);

gs_stagesurf_t *(*device_stagesurface_create_nv12)(gs_device_t *device, uint32_t width, uint32_t height);
gs_stagesurf_t *(*device_stagesurface_create_p010)(gs_device_t *device, uint32_t width, uint32_t height);
void (*device_register_loss_callbacks)(gs_device_t *device, const struct gs_device_loss *callbacks);
void (*device_unregister_loss_callbacks)(gs_device_t *device, void *data);
#endif
