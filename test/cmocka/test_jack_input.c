/* linux-jack: the JACK Input Client source.
 *
 * 1. jack_update() keeps the client name as "OBS Studio: <source name>" but
 *    compared it with the bare source name, so every settings update (even
 *    one that changed nothing) closed and reopened the JACK client, dropping
 *    the capture and its port connections.
 * 2. When jack_get_cycle_times() fails, jack_process_callback() guesses the
 *    timestamp from the frame count and a sample rate that was never set
 *    (jack_data.samples_per_sec stays 0): an integer divide by zero.
 *
 * The real plugin code is compiled in; libjack is replaced by the functions
 * below (declared in jack-stubs/jack/jack.h) and libobs's source calls are
 * stubbed with --wrap, so no JACK server, sound card or display is needed. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "plugins/linux-jack/jack-input.c.h"
#include "plugins/linux-jack/jack-wrapper.c.h"

const char *obs_module_text(const char *val)
{
	return val;
}

/* libobs stand-ins */

static const char *source_name = "Mic";
static struct obs_source_audio last_audio;
static int audio_outputs;

const char *__wrap_obs_source_get_name(const obs_source_t *source)
{
	(void)source;
	return source_name;
}

void __wrap_obs_source_set_async_compensation(obs_source_t *source, bool enable)
{
	(void)source;
	(void)enable;
}

void __wrap_obs_source_output_audio(obs_source_t *source, const struct obs_source_audio *audio)
{
	(void)source;
	last_audio = *audio;
	audio_outputs++;
}

/* libjack stand-ins */

struct _jack_client {
	int unused;
};
struct _jack_port {
	int unused;
};

static struct _jack_client fake_client;
static struct _jack_port fake_ports[8];
static float port_buffer[4096];
static int client_opens;
static int client_closes;
static bool cycle_times_fail;

jack_client_t *jack_client_open(const char *client_name, jack_options_t options, jack_status_t *status, ...)
{
	(void)client_name;
	(void)options;
	(void)status;
	client_opens++;
	return &fake_client;
}

int jack_client_close(jack_client_t *client)
{
	(void)client;
	client_closes++;
	return 0;
}

jack_port_t *jack_port_register(jack_client_t *client, const char *port_name, const char *port_type,
				unsigned long flags, unsigned long buffer_size)
{
	(void)client;
	(void)port_type;
	(void)flags;
	(void)buffer_size;
	unsigned index = 0;
	sscanf(port_name, "in_%u", &index);
	return index >= 1 && index <= 8 ? &fake_ports[index - 1] : NULL;
}

int jack_set_process_callback(jack_client_t *client, JackProcessCallback process_callback, void *arg)
{
	(void)client;
	(void)process_callback;
	(void)arg;
	return 0;
}

int jack_activate(jack_client_t *client)
{
	(void)client;
	return 0;
}

jack_nframes_t jack_get_sample_rate(jack_client_t *client)
{
	(void)client;
	return 48000;
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes)
{
	(void)port;
	(void)nframes;
	return port_buffer;
}

int jack_get_cycle_times(const jack_client_t *client, jack_nframes_t *current_frames, jack_time_t *current_usecs,
			 jack_time_t *next_usecs, float *period_usecs)
{
	(void)client;
	*current_frames = 0;
	*current_usecs = 0;
	*next_usecs = 0;
	*period_usecs = 10000.0f;
	return cycle_times_fail ? -1 : 0;
}

static int reset(void **state)
{
	(void)state;
	source_name = "Mic";
	client_opens = 0;
	client_closes = 0;
	cycle_times_fail = false;
	audio_outputs = 0;
	memset(&last_audio, 0, sizeof(last_audio));
	return 0;
}

static obs_data_t *stereo_settings(void)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "channels", 2);
	obs_data_set_bool(settings, "startjack", false);
	return settings;
}

/* Applying the same settings again keeps the running JACK client. */
static void unchanged_update_keeps_client(void **state)
{
	(void)state;
	obs_data_t *settings = stereo_settings();
	void *data = jack_output_capture.create(settings, (obs_source_t *)&fake_client);
	assert_non_null(data);
	assert_int_equal(client_opens, 1);

	jack_output_capture.update(data, settings);
	jack_output_capture.update(data, settings);

	assert_int_equal(client_opens, 1);
	assert_int_equal(client_closes, 0);
	assert_string_equal(((struct jack_data *)data)->device, "OBS Studio: Mic");

	jack_output_capture.destroy(data);
	obs_data_release(settings);
}

/* Renaming the source still renames (reopens) the JACK client. */
static void renamed_source_reopens_client(void **state)
{
	(void)state;
	obs_data_t *settings = stereo_settings();
	void *data = jack_output_capture.create(settings, (obs_source_t *)&fake_client);
	assert_non_null(data);

	source_name = "Guitar";
	jack_output_capture.update(data, settings);

	assert_int_equal(client_opens, 2);
	assert_string_equal(((struct jack_data *)data)->device, "OBS Studio: Guitar");

	jack_output_capture.destroy(data);
	obs_data_release(settings);
}

/* Without cycle times the timestamp is one period (nframes at the server
 * rate) before now; this divided by the never-set samples_per_sec. */
static void fallback_timestamp_uses_server_rate(void **state)
{
	(void)state;
	obs_data_t *settings = stereo_settings();
	void *data = jack_output_capture.create(settings, (obs_source_t *)&fake_client);
	assert_non_null(data);

	cycle_times_fail = true;
	uint64_t before = os_gettime_ns();
	jack_process_callback(480, data);
	uint64_t after = os_gettime_ns();

	assert_int_equal(audio_outputs, 1);
	assert_int_equal(last_audio.samples_per_sec, 48000);
	/* 480 frames at 48 kHz are 10 ms. */
	assert_true(last_audio.timestamp + 10000000ULL >= before);
	assert_true(last_audio.timestamp + 10000000ULL <= after);

	jack_output_capture.destroy(data);
	obs_data_release(settings);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(unchanged_update_keeps_client, reset),
		cmocka_unit_test_setup(renamed_source_reopens_client, reset),
		cmocka_unit_test_setup(fallback_timestamp_uses_server_rate, reset),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
