#pragma once
#include <libavformat/avformat.h>
#include <libavutil/avassert.h>
#include <libavutil/avstring.h>
#include <libavutil/parseutils.h>
#include <libavutil/time.h>
#include <libavutil/error.h>
#include <stdio.h>
#include <time.h>

#define HTTP_PROTO "http"
#define RIST_PROTO "rist"
#define SRT_PROTO "srt"
#define TCP_PROTO "tcp"
#define UDP_PROTO "udp"

/* lightened version of a struct used by avformat */
typedef struct URLContext {
	void *priv_data; /* SRTContext or RISTContext */
	char *url;       /* URL */
	int max_packet_size;
	AVIOInterruptCB interrupt_callback;
	int64_t rw_timeout; /* max time to wait for write completion in mcs */
} URLContext;

#define UDP_DEFAULT_PAYLOAD_SIZE 1316

/* The host's syslog setup for libsrt, wall clock and gai_strerror (#853):
 * hosts/<os>/obs-ffmpeg-net-host.h. */
#include "obs-ffmpeg-net-host.h"

/* We need to override libsrt/logging_api.h due to conflicts with some libobs
 * definitions.
 */

#define INC_SRT_LOGGING_API_H

// These are required for access functions:
// - adding FA (requires set)
// - setting a log stream (requires iostream)
#ifdef __cplusplus
#include <set>
#include <iostream>
#endif

// Syslog is included so that it provides log level names.
// Haivision log standard requires the same names plus extra one:
#ifndef LOG_DEBUG_TRACE
#define LOG_DEBUG_TRACE 8
#endif

// Flags
#define SRT_LOGF_DISABLE_TIME 1
#define SRT_LOGF_DISABLE_THREADNAME 2
#define SRT_LOGF_DISABLE_SEVERITY 4
#define SRT_LOGF_DISABLE_EOL 8

// Handler type
typedef void SRT_LOG_HANDLER_FN(void *opaque, int level, const char *file, int line, const char *area,
				const char *message);

#ifdef __cplusplus
namespace srt_logging
{

	struct LogFA {
		private : int value;

		public : operator int() const { return value; }

		LogFA(int v) : value(v) {}
	};

	const LogFA LOGFA_GENERAL = 0;

	namespace LogLevel
	{
		enum type {
			fatal = LOG_CRIT,
			error = LOG_ERR,
			warning = 4, //issue w/ libobs so LOG_WARNING is removed
			note = LOG_NOTICE,
			debug = 7 //issue w/ libobs so LOG_DEBUG is removed
		};
	}
} // namespace srt_logging
#endif
