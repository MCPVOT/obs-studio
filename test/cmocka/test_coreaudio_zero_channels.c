#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <cmocka.h>

#include "coreaudio-format.h"

/* A pure output device answers the CoreAudio stream-format query with noErr and
 * zero channels. Taking that count into bzalloc(0) crashed libobs (#2482), so
 * the count is validated before anything allocates from it. */

static void rejected_when_the_device_reports_no_channels(void **state)
{
	uint32_t channels = 0xdeadbeef;

	assert_false(coreaudio_capture_format(0, 64, &channels));
	/* A rejected format must not be written back: the caller keeps its own
	 * state and fails initialization. */
	assert_int_equal(channels, 0xdeadbeef);

	(void)state;
}

static void accepted_when_the_device_reports_channels(void **state)
{
	uint32_t channels = 0;

	assert_true(coreaudio_capture_format(2, 64, &channels));
	assert_int_equal(channels, 2);

	assert_true(coreaudio_capture_format(1, 64, &channels));
	assert_int_equal(channels, 1);

	(void)state;
}

static void clamped_to_the_plugin_maximum(void **state)
{
	uint32_t channels = 0;

	assert_true(coreaudio_capture_format(16, 8, &channels));
	assert_int_equal(channels, 8);

	/* Exactly at the maximum is not clamped. */
	assert_true(coreaudio_capture_format(64, 64, &channels));
	assert_int_equal(channels, 64);

	(void)state;
}

static void rejected_without_a_channel_budget(void **state)
{
	uint32_t channels = 0xdeadbeef;

	assert_false(coreaudio_capture_format(2, 0, &channels));
	assert_int_equal(channels, 0xdeadbeef);

	(void)state;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(rejected_when_the_device_reports_no_channels),
		cmocka_unit_test(accepted_when_the_device_reports_channels),
		cmocka_unit_test(clamped_to_the_plugin_maximum),
		cmocka_unit_test(rejected_without_a_channel_budget),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
