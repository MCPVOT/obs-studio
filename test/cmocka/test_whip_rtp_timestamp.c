#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include "whip-rtp-timestamp.h"

/* A WHIP stream at 23.976 fps has video RTP timestamps that run 66.6 ppm
 * fast: ~4 ms a minute, ~240 ms an hour against audio, so lip sync drifts
 * on receivers that do not resync from RTCP sender reports (upstream
 * obsproject/obs-studio#13970, reported with measurements by DCColor).
 *
 * WHIPOutput::Data hands each packet's dts_usec delta to Send, which
 * rounds it to the 90 kHz clock and adds it to the previous timestamp.
 * dts_usec is whole microseconds, so a 23.976 fps frame lasts 41708 or
 * 41709 us = 3753.72 or 3753.81 ticks; both round to 3754 instead of
 * averaging 3753.75, and the rounding error accumulates. At 59.94 fps the
 * error goes the other way (-111 ppm).
 *
 * A packet's RTP timestamp must stay within a tick of its dts on the RTP
 * clock, however long the stream runs. */

#define VIDEO_CLOCK 90000
#define OPUS_CLOCK 48000
#define USEC 1000000LL

/* libobs: an encoder's packet dts counts in timebase_num steps, and
 * packet_dts_usec() is dts * 1000000 / timebase_den, truncated. */
static int64_t packet_dts_usec(int64_t dts, int64_t timebase_den)
{
	return dts * USEC / timebase_den;
}

/* The exact RTP time of a dts (timebase_num / timebase_den seconds per
 * unit), rounded to the nearest tick and wrapped like an RTP timestamp. */
static uint32_t exact_rtp(int64_t dts, int64_t timebase_num, int64_t timebase_den, uint32_t clock_rate)
{
	int64_t num = dts * timebase_num * clock_rate;
	return (uint32_t)((2 * num + timebase_den) / (2 * timebase_den));
}

/* Feeds `packets` packets of `step` dts units each, starting at dts
 * `first`, through one RTP stream and checks every timestamp. */
static void check_stream(int64_t first, int64_t step, int64_t packets, int64_t timebase_num, int64_t timebase_den,
			 uint32_t clock_rate)
{
	struct whip_rtp_clock clock = {0};
	int32_t worst = 0;
	int64_t worst_packet = 0;

	for (int64_t i = 0; i < packets; i++) {
		int64_t dts = first + i * step;
		uint32_t ts =
			whip_rtp_clock_next(&clock, packet_dts_usec(dts * timebase_num, timebase_den), clock_rate);
		int32_t err = (int32_t)(ts - exact_rtp(dts, timebase_num, timebase_den, clock_rate));

		if (err > worst || -err > worst) {
			worst = err < 0 ? -err : err;
			worst_packet = i;
		}
	}

	if (worst > 1) {
		int64_t usec = worst_packet * step * timebase_num * USEC / timebase_den;
		fprintf(stderr,
			"%lld/%lld s per unit, %u Hz clock: RTP timestamp is %d ticks (%.1f ms) off after %lld packets "
			"(%.1f s)\n",
			(long long)timebase_num, (long long)timebase_den, clock_rate, worst,
			worst * 1000.0 / clock_rate, (long long)worst_packet, usec / 1e6);
		fail();
	}
}

/* An hour of video frames at fps_num/fps_den. libobs gives video packets a
 * timebase of fps_den/fps_num, so each frame is one dts unit. */
static void check_video_hour(int64_t fps_num, int64_t fps_den)
{
	int64_t frames = 3600 * fps_num / fps_den;

	check_stream(0, 1, frames, fps_den, fps_num, VIDEO_CLOCK);
}

static void test_video_23_976_fps_stays_on_the_clock(void **state)
{
	(void)state;
	check_video_hour(24000, 1001);
}

static void test_video_59_94_fps_stays_on_the_clock(void **state)
{
	(void)state;
	check_video_hour(60000, 1001);
}

static void test_video_47_952_fps_stays_on_the_clock(void **state)
{
	(void)state;
	check_video_hour(48000, 1001);
}

/* Rates whose frames already last whole ticks, or round to them exactly. */
static void test_video_common_rates_stay_on_the_clock(void **state)
{
	(void)state;
	check_video_hour(24, 1);
	check_video_hour(25, 1);
	check_video_hour(30, 1);
	check_video_hour(30000, 1001);
	check_video_hour(50, 1);
	check_video_hour(60, 1);
	check_video_hour(120, 1);
}

/* 20 ms Opus packets: 960 samples, timebase 1/48000. */
static void test_opus_audio_stays_on_the_clock(void **state)
{
	(void)state;
	check_stream(0, 960, 3600 * 50, 1, 48000, OPUS_CLOCK);
}

/* A stream that does not start at dts 0 and crosses the 32-bit RTP
 * timestamp wrap (every 13.25 hours at 90 kHz). */
static void test_video_crosses_the_timestamp_wrap(void **state)
{
	(void)state;
	int64_t wrap_frame = (((int64_t)1 << 32) / VIDEO_CLOCK) * 24000 / 1001;

	check_stream(wrap_frame - 600, 1, 1200, 1001, 24000, VIDEO_CLOCK);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_video_23_976_fps_stays_on_the_clock),
		cmocka_unit_test(test_video_59_94_fps_stays_on_the_clock),
		cmocka_unit_test(test_video_47_952_fps_stays_on_the_clock),
		cmocka_unit_test(test_video_common_rates_stay_on_the_clock),
		cmocka_unit_test(test_opus_audio_stays_on_the_clock),
		cmocka_unit_test(test_video_crosses_the_timestamp_wrap),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
