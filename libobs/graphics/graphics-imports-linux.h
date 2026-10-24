/* libobs/graphics' Linux and BSD imports (obs-rust/obs-studio#872). Included inside
 * gs_load_graphics_imports() after GRAPHICS_IMPORT and
 * GRAPHICS_IMPORT_OPTIONAL are defined; no include guard. */

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__)
GRAPHICS_IMPORT(device_texture_create_from_dmabuf);
GRAPHICS_IMPORT(device_query_dmabuf_capabilities);
GRAPHICS_IMPORT(device_query_dmabuf_modifiers_for_format);
GRAPHICS_IMPORT(device_texture_create_from_pixmap);
GRAPHICS_IMPORT(device_query_sync_capabilities);
GRAPHICS_IMPORT(device_sync_create);
GRAPHICS_IMPORT(device_sync_create_from_syncobj_timeline_point);
GRAPHICS_IMPORT(device_sync_destroy);
GRAPHICS_IMPORT(device_sync_export_syncobj_timeline_point);
GRAPHICS_IMPORT(device_sync_signal_syncobj_timeline_point);
GRAPHICS_IMPORT(device_sync_wait);
#endif
