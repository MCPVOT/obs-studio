#pragma once

#include <math.h>
#include <stdint.h>

/*
 * RTP timestamp of each packet a WHIP output sends on one RTP stream (the
 * audio track, or one video simulcast layer). Packets arrive with their
 * libobs dts in microseconds; the RTP clock runs at clock_rate (90 kHz for
 * video, 48 kHz for Opus).
 */
struct whip_rtp_clock {
	uint32_t timestamp; /* RTP timestamp of the last packet */
	int64_t last_usec;  /* dts_usec of the last packet */
};

static inline uint32_t whip_rtp_clock_next(struct whip_rtp_clock *clock, int64_t dts_usec, uint32_t clock_rate)
{
	/* Each packet's duration, rounded to the RTP clock like
	 * rtc::RtpPacketizationConfig::secondsToTimestamp, added to the
	 * previous timestamp. */
	double elapsed_seconds = (double)(dts_usec - clock->last_usec) / 1000000.0;
	uint32_t elapsed = (uint32_t)(int64_t)round(elapsed_seconds * (double)clock_rate);

	clock->timestamp += elapsed;
	clock->last_usec = dts_usec;
	return clock->timestamp;
}
