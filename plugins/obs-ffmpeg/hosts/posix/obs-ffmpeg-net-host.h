/* obs-ffmpeg's macOS and Linux network host (obs-rust/obs-studio#853),
 * found through the hosts/posix include directory by obs-ffmpeg-url.h. */

#pragma once

#include <netdb.h>
#include <time.h>

/* Keep the system syslog.h out of libsrt's logging header: its LOG_* names
 * clash with libobs's. */
#define _SYS_SYSLOG_H
#include <syslog.h>

/* Wall-clock seconds, for the SRT output's statistics interval. */
static inline double obs_ffmpeg_host_wall_time(void)
{
	struct timespec timesp;
	clock_gettime(CLOCK_REALTIME, &timesp);
	return (double)timesp.tv_sec + 0.000000001 * (double)timesp.tv_nsec;
}

static inline const char *obs_ffmpeg_host_gai_strerror(int error)
{
	return gai_strerror(error);
}
