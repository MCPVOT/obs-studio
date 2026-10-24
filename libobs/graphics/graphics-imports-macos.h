/* libobs/graphics' macOS imports (obs-rust/obs-studio#872). Included inside
 * gs_load_graphics_imports() after GRAPHICS_IMPORT and
 * GRAPHICS_IMPORT_OPTIONAL are defined; no include guard. */

#ifdef __APPLE__
GRAPHICS_IMPORT(device_shared_texture_available);
GRAPHICS_IMPORT(device_texture_open_shared);
GRAPHICS_IMPORT(device_texture_create_from_iosurface);
GRAPHICS_IMPORT(gs_texture_rebind_iosurface);
#endif
