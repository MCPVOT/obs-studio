/* libobs/graphics' macOS entry points in struct gs_exports (obs-rust/obs-studio#872). Included inside
 * struct gs_exports; no include guard. */

#ifdef __APPLE__
/* OSX/Cocoa specific functions */
gs_texture_t *(*device_texture_create_from_iosurface)(gs_device_t *dev, void *iosurf);
gs_texture_t *(*device_texture_open_shared)(gs_device_t *dev, uint32_t handle);
bool (*gs_texture_rebind_iosurface)(gs_texture_t *texture, void *iosurf);
bool (*device_shared_texture_available)(void);
#endif
