/* #1275: a busy Lua source must not stall the graphics thread. Compile the
 * production helper with real pthread mutexes. Context and sleep are hooks;
 * no CPU-time or Windows scheduler claim follows from this test. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdbool.h>
#include <pthread.h>

static pthread_mutex_t script_mutex;
static bool graphics_context;
static volatile long lua_graphics_waiters;
static unsigned int attempts;
static unsigned int sleeps;
static unsigned int blocking_calls;

static void *test_context(void)
{
	return graphics_context ? &script_mutex : NULL;
}

static int test_trylock(pthread_mutex_t *mutex)
{
	attempts++;
	return pthread_mutex_trylock(mutex);
}

static int test_lock(pthread_mutex_t *mutex)
{
	blocking_calls++;
	return pthread_mutex_lock(mutex);
}

static long test_waiters(volatile long *value)
{
	return *value;
}

static void test_sleep(unsigned int milliseconds)
{
	(void)milliseconds;
	sleeps++;
	/* Let the unfixed helper exit on its next iteration. The assertions
	 * below still fail, but the RED run never needs a timing watchdog. */
	lua_graphics_waiters = 1;
}

#define gs_get_context test_context
#define pthread_mutex_trylock test_trylock
#define pthread_mutex_lock test_lock
#define os_atomic_load_long test_waiters
#define os_sleep_ms test_sleep
#include "lua-graphics-lock-bounded-under-test.h"
#undef gs_get_context
#undef pthread_mutex_trylock
#undef pthread_mutex_lock
#undef os_atomic_load_long
#undef os_sleep_ms

static int setup(void **state)
{
	(void)state;
	graphics_context = true;
	lua_graphics_waiters = 0;
	attempts = sleeps = blocking_calls = 0;
	return pthread_mutex_init(&script_mutex, NULL);
}

static int teardown(void **state)
{
	(void)state;
	return pthread_mutex_destroy(&script_mutex);
}

/* The owner holds the script mutex until explicitly released by the test.
 * A separate condition variable establishes contention without sleeps. */
struct owner {
	pthread_mutex_t gate;
	pthread_cond_t condition;
	bool ready;
	bool release;
};

static void *hold_script_lock(void *arg)
{
	struct owner *owner = arg;
	pthread_mutex_lock(&script_mutex);
	pthread_mutex_lock(&owner->gate);
	owner->ready = true;
	pthread_cond_signal(&owner->condition);
	while (!owner->release)
		pthread_cond_wait(&owner->condition, &owner->gate);
	pthread_mutex_unlock(&owner->gate);
	pthread_mutex_unlock(&script_mutex);
	return NULL;
}

static void check_contention(long waiters, unsigned int calls)
{
	struct owner owner = {0};
	pthread_t thread;
	assert_int_equal(pthread_mutex_init(&owner.gate, NULL), 0);
	assert_int_equal(pthread_cond_init(&owner.condition, NULL), 0);
	assert_int_equal(pthread_create(&thread, NULL, hold_script_lock, &owner), 0);
	pthread_mutex_lock(&owner.gate);
	while (!owner.ready)
		pthread_cond_wait(&owner.condition, &owner.gate);
	pthread_mutex_unlock(&owner.gate);

	bool all_skipped = true;
	for (unsigned int i = 0; i < calls; i++) {
		lua_graphics_waiters = waiters;
		if (lock_in_graphics(&script_mutex)) {
			all_skipped = false;
			pthread_mutex_unlock(&script_mutex);
		}
	}

	pthread_mutex_lock(&owner.gate);
	owner.release = true;
	pthread_cond_signal(&owner.condition);
	pthread_mutex_unlock(&owner.gate);
	assert_int_equal(pthread_join(thread, NULL), 0);
	assert_int_equal(pthread_cond_destroy(&owner.condition), 0);
	assert_int_equal(pthread_mutex_destroy(&owner.gate), 0);

	assert_true(all_skipped);
	assert_int_equal(sleeps, 0);
	assert_int_equal(attempts, calls);
	assert_int_equal(blocking_calls, 0);
	/* Skipping must neither acquire nor corrupt the lock; the next frame
	 * can acquire it after the owner finishes. */
	assert_true(lock_in_graphics(&script_mutex));
	assert_int_equal(pthread_mutex_unlock(&script_mutex), 0);
}

static void busy_without_graphics_waiter_skips(void **state)
{
	(void)state;
	check_contention(0, 1);
}

static void busy_with_graphics_waiter_skips(void **state)
{
	(void)state;
	check_contention(1, 1);
}

static void repeated_busy_frames_do_not_poll(void **state)
{
	(void)state;
	check_contention(0, 10000);
}

static void free_graphics_lock_acquires(void **state)
{
	(void)state;
	lua_graphics_waiters = 1;
	assert_true(lock_in_graphics(&script_mutex));
	assert_int_equal(pthread_mutex_unlock(&script_mutex), 0);
	assert_int_equal(attempts, 1);
	assert_int_equal(sleeps, 0);
	assert_int_equal(blocking_calls, 0);
}

static void outside_graphics_keeps_blocking_path(void **state)
{
	(void)state;
	graphics_context = false;
	assert_true(lock_in_graphics(&script_mutex));
	assert_int_equal(pthread_mutex_unlock(&script_mutex), 0);
	assert_int_equal(blocking_calls, 1);
	assert_int_equal(attempts, 0);
	assert_int_equal(sleeps, 0);
}

int main(void)
{
	/* Hooks used by the old implementation remain for reproducible RED. */
	(void)test_waiters;
	(void)test_sleep;
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(busy_without_graphics_waiter_skips, setup, teardown),
		cmocka_unit_test_setup_teardown(busy_with_graphics_waiter_skips, setup, teardown),
		cmocka_unit_test_setup_teardown(repeated_busy_frames_do_not_poll, setup, teardown),
		cmocka_unit_test_setup_teardown(free_graphics_lock_acquires, setup, teardown),
		cmocka_unit_test_setup_teardown(outside_graphics_keeps_blocking_path, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
