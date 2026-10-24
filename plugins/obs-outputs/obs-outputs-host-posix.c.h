#include "plugins/obs-outputs/build/obs-outputs-host-posix.pch.h"

/* obs-outputs' macOS and Linux module host (#851): sockets and mbedtls need
 * no setup. */

void obs_outputs_host_load(void) {}

void obs_outputs_host_unload(void) {}
