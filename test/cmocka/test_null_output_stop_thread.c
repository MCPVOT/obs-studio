/* obs-outputs: the null output (used to measure encoders, e.g. by the
 * auto-configuration wizard) ends data capture on a stop thread. That thread
 * cleared stop_thread_active itself when it finished, so the next start and
 * the destroy saw no thread to join: every completed stop left a joinable
 * thread that was never joined, leaking its stack and handle.
 *
 * The real null-output.c is compiled in. pthread_create and pthread_join are
 * wrapped (--wrap) to count the stop threads and their joins and to report
 * when a stop thread has finished; libobs's output calls are stubbed, so no
 * encoder, video or display is needed. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <semaphore.h>
#include <stdlib.h>

#include "plugins/obs-outputs/null-output.c.h"

/* libobs stand-ins */

static int end_capture_calls;

bool __wrap_obs_output_can_begin_data_capture(const obs_output_t *output, uint32_t flags)
{
	(void)output;
	(void)flags;
	return true;
}

bool __wrap_obs_output_initialize_encoders(obs_output_t *output, uint32_t flags)
{
	(void)output;
	(void)flags;
	return true;
}

bool __wrap_obs_output_begin_data_capture(obs_output_t *output, uint32_t flags)
{
	(void)output;
	(void)flags;
	return true;
}

void __wrap_obs_output_end_data_capture(obs_output_t *output)
{
	(void)output;
	end_capture_calls++;
}

/* pthread wrappers */

static sem_t thread_finished;
static int threads_created;
static int threads_joined;

struct counted_start {
	void *(*routine)(void *);
	void *arg;
};

static void *counted_thread(void *param)
{
	struct counted_start start = *(struct counted_start *)param;
	free(param);
	void *result = start.routine(start.arg);
	sem_post(&thread_finished);
	return result;
}

int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg)
{
	struct counted_start *start = malloc(sizeof(*start));
	start->routine = routine;
	start->arg = arg;
	int err = __real_pthread_create(thread, attr, counted_thread, start);
	if (err == 0)
		threads_created++;
	else
		free(start);
	return err;
}

int __real_pthread_join(pthread_t thread, void **result);
int __wrap_pthread_join(pthread_t thread, void **result)
{
	int err = __real_pthread_join(thread, result);
	if (err == 0)
		threads_joined++;
	return err;
}

/* Stops the output and waits until its stop thread has returned. */
static void stop_and_wait(void *data)
{
	/* Parenthesized: cmocka.h defines a stop() macro. */
	(null_output_info.stop)(data, 0);
	assert_int_equal(sem_wait(&thread_finished), 0);
}

/* Two encoding sessions, each stop thread finished before the next start:
 * both stop threads are joined, one by the second start and one by destroy. */
static void completed_stop_threads_are_joined(void **state)
{
	(void)state;
	assert_int_equal(sem_init(&thread_finished, 0, 0), 0);

	void *data = null_output_info.create(NULL, (obs_output_t *)&thread_finished);
	assert_non_null(data);

	assert_true(null_output_info.start(data));
	stop_and_wait(data);
	assert_true(null_output_info.start(data));
	assert_int_equal(threads_joined, 1);
	stop_and_wait(data);

	null_output_info.destroy(data);

	assert_int_equal(end_capture_calls, 2);
	assert_int_equal(threads_created, 2);
	assert_int_equal(threads_joined, 2);
	sem_destroy(&thread_finished);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(completed_stop_threads_are_joined),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
