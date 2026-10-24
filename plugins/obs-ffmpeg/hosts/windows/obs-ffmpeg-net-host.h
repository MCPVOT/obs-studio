/* obs-ffmpeg's Windows network host (obs-rust/obs-studio#853), found
 * through the hosts/windows include directory by obs-ffmpeg-url.h. */

#pragma once

#include <sys/timeb.h>
#include <ws2tcpip.h>

/* We need to override libsrt/win/syslog_defs.h due to conflicts w/ some libobs
 * definitions.
 */
#ifndef INC_SRT_WINDOWS_SYSLOG_DEFS_H
#define INC_SRT_WINDOWS_SYSLOG_DEFS_H

#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
//#define LOG_WARNING 4 // this creates issues w/ libobs LOG_WARNING = 200
#define LOG_NOTICE 5
//#define LOG_INFO 6 // issue w/ libobs
//#define LOG_DEBUG 7 // issue w/ libobs
#define LOG_PRIMASK 0x07

#define LOG_PRI(p) ((p) & LOG_PRIMASK)
#define LOG_MAKEPRI(fac, pri) (((fac) << 3) | (pri))

#define LOG_KERN (0 << 3)
#define LOG_USER (1 << 3)
#define LOG_MAIL (2 << 3)
#define LOG_DAEMON (3 << 3)
#define LOG_AUTH (4 << 3)
#define LOG_SYSLOG (5 << 3)
#define LOG_LPR (6 << 3)
#define LOG_NEWS (7 << 3)
#define LOG_UUCP (8 << 3)
#define LOG_CRON (9 << 3)
#define LOG_AUTHPRIV (10 << 3)
#define LOG_FTP (11 << 3)

/* Codes through 15 are reserved for system use */
#define LOG_LOCAL0 (16 << 3)
#define LOG_LOCAL1 (17 << 3)
#define LOG_LOCAL2 (18 << 3)
#define LOG_LOCAL3 (19 << 3)
#define LOG_LOCAL4 (20 << 3)
#define LOG_LOCAL5 (21 << 3)
#define LOG_LOCAL6 (22 << 3)
#define LOG_LOCAL7 (23 << 3)

#define LOG_NFACILITIES 24
#define LOG_FACMASK 0x03f8
#define LOG_FAC(p) (((p) & LOG_FACMASK) >> 3)
#endif

/* Wall-clock seconds, for the SRT output's statistics interval. */
static inline double obs_ffmpeg_host_wall_time(void)
{
	struct timeb timebuffer;
	ftime(&timebuffer);
	return (double)timebuffer.time + 0.001 * (double)timebuffer.millitm;
}

static inline const char *obs_ffmpeg_host_gai_strerror(int error)
{
	return gai_strerrorA(error);
}
