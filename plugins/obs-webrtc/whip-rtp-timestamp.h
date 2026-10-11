#pragma once

#include <stdint.h>

/*
 * RTP timestamp of each packet a WHIP output sends on one RTP stream (the
 * audio track, or one video simulcast layer). Packets arrive with their
 * libobs dts in microseconds; the RTP clock runs at clock_rate (90 kHz for
 * video, 48 kHz for Opus).
 */
struct whip_rtp_clock {
	uint32_t start;     /* RTP timestamp of dts 0 */
	uint32_t timestamp; /* RTP timestamp of the last packet */
};

static inline uint32_t whip_rtp_clock_next(struct whip_rtp_clock *clock, int64_t dts_usec, uint32_t clock_rate)
{
	/* Rescale the packet's own dts to the RTP clock, rounded to the
	 * nearest tick, rather than adding up rounded per-packet durations:
	 * those are whole microseconds, so at rates like 23.976 or 59.94 fps
	 * every frame rounds the same way and the error accumulates. Split
	 * into seconds and microseconds so the product cannot overflow, with
	 * floor division so a negative dts rounds the same way. The result
	 * wraps modulo 2^32 like any RTP timestamp. */
	int64_t sec = dts_usec / 1000000;
	int64_t usec = dts_usec % 1000000;

	if (usec < 0) {
		usec += 1000000;
		sec -= 1;
	}

	clock->timestamp = clock->start +
			   (uint32_t)((uint64_t)sec * clock_rate + (uint64_t)((usec * clock_rate + 500000) / 1000000));
	return clock->timestamp;
}
