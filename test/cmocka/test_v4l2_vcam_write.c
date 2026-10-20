/* linux-v4l2: the virtual camera writes each frame to the v4l2loopback device
 * with write(). A write may take only part of the frame (a short write) or be
 * interrupted before it writes anything (EINTR). virtual_video() resent the
 * whole frame from its first byte after a short write, so the device got the
 * start of the frame again where the rest belonged, and it dropped the frame
 * on EINTR.
 *
 * The real v4l2-output.c is compiled in and write() is replaced with --wrap
 * by one that takes at most a few bytes per call, so no v4l2loopback device
 * is needed. */

/* v4l2-output.c needs it (versionsort) before the first system header. */
#define _GNU_SOURCE

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "plugins/linux-v4l2/v4l2-output.c.h"

#define FRAME_SIZE 1000
#define MAX_CALLS 64

static uint8_t device_bytes[FRAME_SIZE * 4];
static size_t device_len;
static size_t chunk;
static int calls;
static int eintr_left;

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
	(void)fd;
	/* Bounds the old loop, which could count its way past zero. */
	if (++calls > MAX_CALLS) {
		errno = EIO;
		return -1;
	}
	if (eintr_left > 0) {
		eintr_left--;
		errno = EINTR;
		return -1;
	}
	size_t n = count < chunk ? count : chunk;
	if (n > sizeof(device_bytes) - device_len)
		n = sizeof(device_bytes) - device_len;
	memcpy(device_bytes + device_len, buf, n);
	device_len += n;
	return (ssize_t)n;
}

static uint8_t frame_bytes[FRAME_SIZE];

static void write_frame(size_t write_chunk, int interruptions)
{
	struct virtualcam_data vcam = {0};
	struct video_data frame = {0};

	for (size_t i = 0; i < FRAME_SIZE; i++)
		frame_bytes[i] = (uint8_t)(i * 7 + 3);
	device_len = 0;
	calls = 0;
	chunk = write_chunk;
	eintr_left = interruptions;

	vcam.device = 42;
	vcam.frame_size = FRAME_SIZE;
	frame.data[0] = frame_bytes;
	frame.linesize[0] = FRAME_SIZE;

	virtualcam_info.raw_video(&vcam, &frame);
}

/* A frame that fits in one write is written once. */
static void whole_frame_in_one_write(void **state)
{
	(void)state;
	write_frame(FRAME_SIZE, 0);
	assert_int_equal(device_len, FRAME_SIZE);
	assert_memory_equal(device_bytes, frame_bytes, FRAME_SIZE);
	assert_int_equal(calls, 1);
}

/* Short writes continue where the last one stopped: the device gets every
 * byte of the frame once, in order. */
static void short_writes_continue_the_frame(void **state)
{
	(void)state;
	write_frame(300, 0);
	assert_int_equal(device_len, FRAME_SIZE);
	assert_memory_equal(device_bytes, frame_bytes, FRAME_SIZE);
	assert_int_equal(calls, 4);
}

/* An interrupted write is retried, not the end of the frame. */
static void interrupted_write_is_retried(void **state)
{
	(void)state;
	write_frame(FRAME_SIZE, 1);
	assert_int_equal(device_len, FRAME_SIZE);
	assert_memory_equal(device_bytes, frame_bytes, FRAME_SIZE);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(whole_frame_in_one_write),
		cmocka_unit_test(short_writes_continue_the_frame),
		cmocka_unit_test(interrupted_write_is_retried),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
