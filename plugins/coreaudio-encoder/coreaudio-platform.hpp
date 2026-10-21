#pragma once

/* Private interface: the owning PCH supplies OSStatus from the selected SDK
 * and DStr. Host helpers and encoder share one per-host unity translation unit. */
bool ca_platform_load();
void ca_platform_unload();
bool ca_platform_describe_status(OSStatus code, DStr &result);
