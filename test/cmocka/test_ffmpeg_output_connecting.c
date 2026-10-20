/* ffmpeg_output_start() (plugins/obs-ffmpeg/obs-ffmpeg-output.c.h) started
 * its connect worker and only then stored `connecting = (ret == 0)`. The
 * worker clears `connecting` when it finishes; when it finished before
 * pthread_create() returned (a connection that fails at once), the start
 * path set it back to true, and every later start was refused as "already
 * connecting" (split from #1464).
 *
 * That ordering is a thread race, so the test makes it deterministic:
 * pthread_create() is replaced for the included file by a stand-in that does
 * what a worker which already finished has done (clear `connecting`) before
 * returning success, without running the real connect. The output must then
 * not be left connecting, and a second start must be accepted.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <string.h>

#include "plugins/obs-ffmpeg/build/obs-ffmpeg-output.pch.h"

static int create_calls;
static int create_result;

static int test_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg);

const char *obs_module_text(const char *text)
{
	return text;
}

#define pthread_create test_pthread_create
#include "plugins/obs-ffmpeg/obs-ffmpeg-output.c.h"
#undef pthread_create

static int test_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start)(void *), void *arg)
{
	(void)attr;
	assert_ptr_equal(start, start_thread);
	create_calls++;
	if (create_result != 0)
		return create_result;

	/* The worker ran to completion before pthread_create() returned. */
	((struct ffmpeg_output *)arg)->connecting = false;
	*thread = pthread_self();
	return 0;
}

static void test_worker_finishes_before_create_returns(void **state)
{
	(void)state;
	struct ffmpeg_output output;
	memset(&output, 0, sizeof(output));
	create_calls = 0;
	create_result = 0;

	assert_true(ffmpeg_output_start(&output));
	assert_false(output.connecting);

	/* A finished attempt does not block the next one. */
	assert_true(ffmpeg_output_start(&output));
	assert_int_equal(create_calls, 2);
}

static void test_create_failure_clears_connecting(void **state)
{
	(void)state;
	struct ffmpeg_output output;
	memset(&output, 0, sizeof(output));
	create_calls = 0;
	create_result = EAGAIN;

	assert_false(ffmpeg_output_start(&output));
	assert_false(output.connecting);
	assert_int_equal(create_calls, 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_worker_finishes_before_create_returns),
		cmocka_unit_test(test_create_failure_clears_connecting),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
