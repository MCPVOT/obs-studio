#include "plugins/obs-webrtc/build/obs-webrtc-posix.pch.hpp"

const char *whip_host_os_name(void)
{
	struct utsname u;

	if (uname(&u) == 0) {
		if (strcmp(u.sysname, "OpenBSD") == 0) {
			return "OpenBSD";
		}
		if (strcmp(u.sysname, "FreeBSD") == 0) {
			return "FreeBSD";
		}
	}
	/* Linux and anything else. The old chain said "Linux x86_64" for any
	 * 64-bit Linux (__linux__ && __LP64__), whatever the architecture. */
	return sizeof(void *) == 8 ? "Linux x86_64" : "Linux";
}
