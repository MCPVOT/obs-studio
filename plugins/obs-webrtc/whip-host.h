/* Host interface of the WHIP output (obs-rust/obs-studio#861). Each host
 * implements it in whip-host-<os>.cpp.hpp, built in its own unity group that
 * CMakeLists.txt picks per OS; the shared files call only this. */

#pragma once

/* The operating system named in the WHIP User-Agent header: "Windows x86_64",
 * "Mac OS X", "OpenBSD", "FreeBSD", "Linux x86_64" (64-bit) or "Linux".
 * Returns a static string. */
const char *whip_host_os_name(void);
