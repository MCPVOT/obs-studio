/* The deactivate callback must observe an inactive output. Checking inside
 * the synchronous callback makes the former signal-before-store race
 * deterministic, without sleeps or changing the existing delay tests. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <obs.h>
#include <util/threading.h>

struct observation {
	os_event_t *deactivated;
	bool active_in_callback;
};

static const char *output_name(void *unused)
{
	(void)unused;
	return "deactivate state regression";
}

static void *output_create(obs_data_t *settings, obs_output_t *output)
{
	(void)settings;
	return output;
}

static void output_destroy(void *data)
{
	(void)data;
}

static bool output_start(void *data)
{
	return obs_output_can_begin_data_capture(data, 0) && obs_output_begin_data_capture(data, 0);
}

static void output_stop(void *data, uint64_t timestamp)
{
	(void)timestamp;
	obs_output_end_data_capture(data);
}

static void output_packet(void *data, struct encoder_packet *packet)
{
	(void)data;
	(void)packet;
}

static void on_deactivate(void *data, calldata_t *params)
{
	struct observation *seen = data;
	seen->active_in_callback = obs_output_active(calldata_ptr(params, "output"));
	os_event_signal(seen->deactivated);
}

static void quiet_log(int level, const char *message, va_list args, void *data)
{
	(void)level;
	(void)message;
	(void)args;
	(void)data;
}

static int setup(void **state)
{
	(void)state;
	base_set_log_handler(quiet_log, NULL);
	if (!obs_startup("en-US", NULL, NULL))
		return -1;
	const struct obs_output_info info = {
		.id = "deactivate_state_test",
		/* No actual media or hardware is needed to exercise capture teardown. */
		.flags = OBS_OUTPUT_ENCODED,
		.get_name = output_name,
		.create = output_create,
		.destroy = output_destroy,
		.start = output_start,
		.stop = output_stop,
		.encoded_packet = output_packet,
	};
	obs_register_output(&info);
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	obs_shutdown();
	return 0;
}

static void check_deactivation(bool force)
{
	for (int iteration = 0; iteration < 100; iteration++) {
		struct observation seen = {0};
		assert_int_equal(os_event_init(&seen.deactivated, OS_EVENT_TYPE_AUTO), 0);
		obs_output_t *output = obs_output_create("deactivate_state_test", "state test", NULL, NULL);
		assert_non_null(output);
		signal_handler_connect(obs_output_get_signal_handler(output), "deactivate", on_deactivate, &seen);
		assert_true(obs_output_start(output));
		assert_true(obs_output_active(output));
		if (force)
			obs_output_force_stop(output);
		else
			obs_output_stop(output);
		int waited = os_event_timedwait(seen.deactivated, 5000);
		/* Release joins shutdown before callback data and event are destroyed. */
		obs_output_release(output);
		os_event_destroy(seen.deactivated);
		assert_int_equal(waited, 0);
		assert_false(seen.active_in_callback);
	}
}

static void normal_stop_is_inactive_in_deactivate(void **state)
{
	(void)state;
	check_deactivation(false);
}

static void force_stop_is_inactive_in_deactivate(void **state)
{
	(void)state;
	check_deactivation(true);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(normal_stop_is_inactive_in_deactivate),
		cmocka_unit_test(force_stop_is_inactive_in_deactivate),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
