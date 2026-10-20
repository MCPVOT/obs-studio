/* linux-alsa: stopping an ALSA source before its capture thread has started.
 *
 * _alsa_open() started the capture thread and the thread itself set
 * data->listen = true when it began to run. A stop that came first
 * (_alsa_close() sets listen = false, then joins the thread) was overwritten
 * by the thread's own listen = true: the thread captured on forever and the
 * join, so the destroy, removal of the source or OBS's shutdown, never
 * returned.
 *
 * The race is made deterministic: pthread_create() and pthread_join() are
 * wrapped (--wrap), and the capture thread is held at its start until the
 * stopping thread has set listen = false and is joining it, the order in
 * which the bug hangs. The real capture code (plugins/linux-alsa/alsa-input.c)
 * runs on ALSA's null PCM, so no sound card or display is needed. destroy runs
 * on a helper thread with a deadline. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

extern struct obs_source_info alsa_input_capture;

const char *obs_module_text(const char *val)
{
	return val;
}

void __wrap_obs_source_set_async_compensation(obs_source_t *source, bool enable)
{
	(void)source;
	(void)enable;
}

void __wrap_obs_source_output_audio(obs_source_t *source, const struct obs_source_audio *audio)
{
	(void)source;
	(void)audio;
}

/* The capture thread waits on start_gate until a join releases it. */
static sem_t start_gate;
static volatile bool hold_next_thread;
static volatile bool gate_open;
static volatile long held_threads;

struct held_start {
	void *(*routine)(void *);
	void *arg;
};

static void *held_thread(void *param)
{
	struct held_start start = *(struct held_start *)param;
	free(param);
	sem_wait(&start_gate);
	return start.routine(start.arg);
}

int __real_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*routine)(void *), void *arg)
{
	if (!os_atomic_set_bool(&hold_next_thread, false))
		return __real_pthread_create(thread, attr, routine, arg);

	struct held_start *start = malloc(sizeof(*start));
	start->routine = routine;
	start->arg = arg;
	os_atomic_inc_long(&held_threads);
	return __real_pthread_create(thread, attr, held_thread, start);
}

int __real_pthread_join(pthread_t thread, void **result);
int __wrap_pthread_join(pthread_t thread, void **result)
{
	/* The stopper has set listen = false: let the capture thread start. */
	if (!os_atomic_set_bool(&gate_open, true))
		sem_post(&start_gate);
	return __real_pthread_join(thread, result);
}

struct destroy_call {
	void *data;
	volatile bool done;
};

static void *destroy_thread(void *arg)
{
	struct destroy_call *call = arg;
	alsa_input_capture.destroy(call->data);
	os_atomic_set_bool(&call->done, true);
	return NULL;
}

/* Destroys the source on a helper thread; false when destroy did not
 * return within 2 s (the helper thread is then left behind). */
static bool destroy_with_deadline(void *data)
{
	/* Left to leak when destroy does not return. */
	struct destroy_call *call = calloc(1, sizeof(*call));
	pthread_t thread;
	uint64_t start = os_gettime_ns();

	call->data = data;
	if (__real_pthread_create(&thread, NULL, destroy_thread, call) != 0)
		return false;
	while (!os_atomic_load_bool(&call->done)) {
		if (os_gettime_ns() - start > 2000000000ULL) {
			pthread_detach(thread);
			return false;
		}
		os_sleep_ms(1);
	}
	__real_pthread_join(thread, NULL);
	free(call);
	return true;
}

/* Destroying the source before its capture thread runs still ends the
 * thread, and destroy returns. */
static void stop_before_capture_thread_starts(void **state)
{
	(void)state;
	assert_int_equal(sem_init(&start_gate, 0, 0), 0);
	gate_open = false;
	held_threads = 0;
	os_atomic_set_bool(&hold_next_thread, true);

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "device_id", "null");
	obs_data_set_int(settings, "rate", 48000);
	void *data = alsa_input_capture.create(settings, NULL);
	obs_data_release(settings);
	assert_non_null(data);
	/* The null PCM opened and its capture thread is the one held. */
	assert_int_equal(os_atomic_load_long(&held_threads), 1);

	if (!destroy_with_deadline(data))
		fail_msg("destroy did not return within 2 s: the capture thread ignored the stop");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(stop_before_capture_thread_starts),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
