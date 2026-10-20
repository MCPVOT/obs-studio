/* Characterization for #1275. Compile the actual private C function, sliced
 * at configure time, with controlled context/lock/atomic/sleep dependencies.
 * This checks control flow and requested sleep durations, not pthread
 * scheduling, rendering, wall time, or Windows Sleep(0) CPU consumption.
 * It characterizes C source only; there is no Rust swap for this helper.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdbool.h>
#include <errno.h>

struct test_mutex {
	bool acquired;
};

static struct test_mutex mutex;
static bool graphics_context;
static volatile long lua_graphics_waiters;
static unsigned int blocking_calls;
static unsigned int try_calls;
static unsigned int waiter_reads;
static unsigned int sleep_calls;
static unsigned int failures_remaining;
static unsigned int waiter_after_sleep;
static int lock_error;

static void *test_get_context(void)
{
	return graphics_context ? &mutex : NULL;
}

static int test_mutex_lock(struct test_mutex *value)
{
	assert_ptr_equal(value, &mutex);
	blocking_calls++;
	value->acquired = true;
	return 0;
}

static int test_mutex_trylock(struct test_mutex *value)
{
	assert_ptr_equal(value, &mutex);
	assert_false(value->acquired);
	/* A broken loop fails rather than hanging the test process. */
	assert_true(++try_calls <= 10002);
	if (failures_remaining) {
		failures_remaining--;
		return lock_error;
	}
	value->acquired = true;
	return 0;
}

static long test_atomic_load(volatile long *value)
{
	assert_ptr_equal(value, &lua_graphics_waiters);
	waiter_reads++;
	return *value;
}

static void test_sleep_ms(unsigned int milliseconds)
{
	assert_false(mutex.acquired);
	assert_int_equal(milliseconds, 1);
	sleep_calls++;
	if (waiter_after_sleep && sleep_calls == waiter_after_sleep)
		lua_graphics_waiters = 1;
}

#define pthread_mutex_t struct test_mutex
#define pthread_mutex_lock test_mutex_lock
#define pthread_mutex_trylock test_mutex_trylock
#define gs_get_context test_get_context
#define os_atomic_load_long test_atomic_load
#define os_sleep_ms test_sleep_ms
#include "lua-graphics-lock-under-test.h"
#undef pthread_mutex_t
#undef pthread_mutex_lock
#undef pthread_mutex_trylock
#undef gs_get_context
#undef os_atomic_load_long
#undef os_sleep_ms

static int setup(void **state)
{
	(void)state;
	mutex.acquired = false;
	graphics_context = true;
	lua_graphics_waiters = 0;
	blocking_calls = 0;
	try_calls = 0;
	waiter_reads = 0;
	sleep_calls = 0;
	failures_remaining = 0;
	waiter_after_sleep = 0;
	lock_error = EBUSY;
	return 0;
}

static void non_graphics_uses_blocking_lock(void **state)
{
	(void)state;
	graphics_context = false;
	lua_graphics_waiters = 1;
	assert_true(lock_in_graphics(&mutex));
	assert_true(mutex.acquired);
	assert_int_equal(blocking_calls, 1);
	assert_int_equal(try_calls, 0);
	assert_int_equal(waiter_reads, 0);
	assert_int_equal(sleep_calls, 0);
}

static void uncontended_acquires_even_with_waiter(void **state)
{
	(void)state;
	lua_graphics_waiters = 1;
	assert_true(lock_in_graphics(&mutex));
	assert_true(mutex.acquired);
	assert_int_equal(blocking_calls, 0);
	assert_int_equal(try_calls, 1);
	assert_int_equal(waiter_reads, 0);
	assert_int_equal(sleep_calls, 0);
}

static void contention_retries_until_acquired(void **state)
{
	(void)state;
	failures_remaining = 3;
	assert_true(lock_in_graphics(&mutex));
	assert_true(mutex.acquired);
	assert_int_equal(blocking_calls, 0);
	assert_int_equal(try_calls, 4);
	assert_int_equal(waiter_reads, 3);
	assert_int_equal(sleep_calls, 3);
}

static void existing_waiter_aborts_without_sleep(void **state)
{
	(void)state;
	failures_remaining = 10;
	lua_graphics_waiters = 1;
	assert_false(lock_in_graphics(&mutex));
	assert_false(mutex.acquired);
	assert_int_equal(blocking_calls, 0);
	assert_int_equal(try_calls, 1);
	assert_int_equal(waiter_reads, 1);
	assert_int_equal(sleep_calls, 0);
}

static void new_waiter_aborts_after_next_failed_try(void **state)
{
	(void)state;
	failures_remaining = 10;
	waiter_after_sleep = 3;
	assert_false(lock_in_graphics(&mutex));
	assert_false(mutex.acquired);
	assert_int_equal(try_calls, 4);
	assert_int_equal(waiter_reads, 4);
	assert_int_equal(sleep_calls, 3);
}

static void acquisition_precedes_new_waiter_check(void **state)
{
	(void)state;
	failures_remaining = 1;
	waiter_after_sleep = 1;
	assert_true(lock_in_graphics(&mutex));
	assert_true(mutex.acquired);
	assert_int_equal(try_calls, 2);
	assert_int_equal(waiter_reads, 1);
	assert_int_equal(sleep_calls, 1);
	assert_int_equal(lua_graphics_waiters, 1);
}

static void long_contention_has_no_retry_budget(void **state)
{
	(void)state;
	/* Characterized, not endorsed: 10,000 failed attempts still request a
	 * 1 ms sleep each, with no backoff or attempt limit. These are simulated
	 * sleeps, not a claim of ten seconds of measured elapsed time. */
	failures_remaining = 10000;
	assert_true(lock_in_graphics(&mutex));
	assert_true(mutex.acquired);
	assert_int_equal(try_calls, 10001);
	assert_int_equal(waiter_reads, 10000);
	assert_int_equal(sleep_calls, 10000);
}

static void non_busy_error_is_also_retried(void **state)
{
	(void)state;
	/* Characterized, not endorsed: any nonzero trylock result is retried. */
	lock_error = EINVAL;
	failures_remaining = 2;
	assert_true(lock_in_graphics(&mutex));
	assert_true(mutex.acquired);
	assert_int_equal(try_calls, 3);
	assert_int_equal(waiter_reads, 2);
	assert_int_equal(sleep_calls, 2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(non_graphics_uses_blocking_lock, setup),
		cmocka_unit_test_setup(uncontended_acquires_even_with_waiter, setup),
		cmocka_unit_test_setup(contention_retries_until_acquired, setup),
		cmocka_unit_test_setup(existing_waiter_aborts_without_sleep, setup),
		cmocka_unit_test_setup(new_waiter_aborts_after_next_failed_try, setup),
		cmocka_unit_test_setup(acquisition_precedes_new_waiter_check, setup),
		cmocka_unit_test_setup(long_contention_has_no_retry_budget, setup),
		cmocka_unit_test_setup(non_busy_error_is_also_retried, setup),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
