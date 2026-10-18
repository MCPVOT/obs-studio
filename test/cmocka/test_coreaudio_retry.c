#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <cmocka.h>

#include "coreaudio-retry.h"

/* The CoreAudio reconnect thread used to retry every 300 ms (default-device
 * change) or 2 s (unplug, failed first init) forever. These tests run the
 * schedule against an always-failing init in simulated time. */

#define SIM_MS 120000UL

struct run {
	unsigned long waits[64];
	size_t count;
	size_t cap_logs;
};

/* Drive the schedule until SIM_MS of simulated time has passed. */
static void simulate(struct coreaudio_retry *r, unsigned long ms, struct run *out)
{
	unsigned long now = 0;
	out->count = 0;
	out->cap_logs = 0;
	while (now < ms && out->count < 64) {
		bool cap_first = false;
		unsigned long wait = coreaudio_retry_next(r, &cap_first);
		assert_true(wait > 0);
		if (cap_first)
			out->cap_logs++;
		out->waits[out->count++] = wait;
		now += wait; /* the init attempt after the wait always fails */
	}
}

static void check_doubling(unsigned long base)
{
	struct coreaudio_retry r;
	struct run run;
	coreaudio_retry_reset(&r, base);
	simulate(&r, SIM_MS, &run);

	unsigned long expect = base;
	for (size_t i = 0; i < run.count; i++) {
		assert_int_equal(run.waits[i], expect);
		expect = expect * 2 > COREAUDIO_RETRY_MAX_MS ? COREAUDIO_RETRY_MAX_MS : expect * 2;
	}
	assert_int_equal(run.waits[run.count - 1], COREAUDIO_RETRY_MAX_MS);
	assert_int_equal(run.cap_logs, 1);
	/* Far fewer attempts than one per base interval. */
	assert_true(run.count < 12);
	assert_true(run.count < SIM_MS / base / 4);
}

static void test_backoff_from_300ms(void **state)
{
	(void)state;
	check_doubling(300);
}

static void test_backoff_from_2s(void **state)
{
	(void)state;
	check_doubling(2000);
}

static void test_device_change_resets_to_base(void **state)
{
	(void)state;
	struct coreaudio_retry r;
	struct run run;
	coreaudio_retry_reset(&r, 2000);
	simulate(&r, SIM_MS, &run);
	assert_int_equal(run.waits[run.count - 1], COREAUDIO_RETRY_MAX_MS);

	/* A device-list or default-device change restarts at the base. */
	coreaudio_retry_reset(&r, 2000);
	bool cap_first = true;
	assert_int_equal(coreaudio_retry_next(&r, &cap_first), 2000);
	assert_false(cap_first);
	assert_int_equal(coreaudio_retry_next(&r, &cap_first), 4000);

	/* The cap is reported again for the new schedule. */
	simulate(&r, SIM_MS, &run);
	assert_int_equal(run.cap_logs, 1);
}

/* The listener callback runs on a CoreAudio thread and may still be running
 * after the source let go: it must find a live context, and the last unref
 * (here the listener's) frees it exactly once. */
static void test_wake_outlives_source(void **state)
{
	(void)state;
	struct coreaudio_wake *w = coreaudio_wake_create();
	assert_non_null(w);
	size_t frees = 0;

	coreaudio_wake_ref(w);            /* registered listener */
	frees += coreaudio_wake_unref(w); /* source destroyed */
	assert_int_equal(frees, 0);

	coreaudio_wake_signal(w); /* late callback */
	assert_true(w->reset);
	assert_int_equal(os_event_try(w->event), 0);

	frees += coreaudio_wake_unref(w); /* listener removed */
	assert_int_equal(frees, 1);
}

static void test_wake_source_last(void **state)
{
	(void)state;
	struct coreaudio_wake *w = coreaudio_wake_create();
	assert_non_null(w);
	coreaudio_wake_ref(w);
	coreaudio_wake_ref(w);
	assert_false(coreaudio_wake_unref(w));
	assert_false(coreaudio_wake_unref(w));
	assert_true(coreaudio_wake_unref(w));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_backoff_from_300ms),
		cmocka_unit_test(test_backoff_from_2s),
		cmocka_unit_test(test_device_change_resets_to_base),
		cmocka_unit_test(test_wake_outlives_source),
		cmocka_unit_test(test_wake_source_last),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
