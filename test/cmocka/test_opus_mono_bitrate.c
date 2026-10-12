/* The FFmpeg Opus encoder failed to start above 256 kbps on a mono track
 * (obsproject/obs-studio#12753, reported by aknotwell), with the misleading
 * error "Failed to open AAC codec: Invalid argument". That breaks WHIP,
 * which only takes Opus. libopus allows at most 256 kbps per channel, and
 * the encoder passed the configured bitrate through unchanged; the Opus
 * property allows up to 1024 kbps for every channel layout.
 *
 * The test loads the real obs-ffmpeg module, creates its ffmpeg_opus
 * encoder on a mono or stereo audio mix, and initializes it through a
 * minimal audio-only output with obs_output_initialize_encoders(), as real
 * outputs do when they start. obs_startup() needs a display on Linux, so the cases skip
 * without one (run under xvfb-run). */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include <obs.h>
#include <obs-module.h>
#include <util/base.h>

#define OUTPUT_ID "opus_mono_bitrate_output"

static bool obs_up;
static bool module_up;

static void quiet_log(int lvl, const char *msg, va_list args, void *p)
{
	(void)p;
	if (lvl <= LOG_WARNING) {
		vfprintf(stderr, msg, args);
		fputc('\n', stderr);
	}
}

struct test_output {
	obs_output_t *output;
	bool initialized;
};

static const char *output_name(void *type_data)
{
	(void)type_data;
	return "opus bitrate test output";
}

static void *output_create(obs_data_t *settings, obs_output_t *output)
{
	(void)settings;
	struct test_output *out = bzalloc(sizeof(*out));
	out->output = output;
	return out;
}

static void output_destroy(void *data)
{
	bfree(data);
}

static bool output_start(void *data)
{
	struct test_output *out = data;
	out->initialized = obs_output_initialize_encoders(out->output, 0);
	return out->initialized;
}

static void output_stop(void *data, uint64_t ts)
{
	(void)data;
	(void)ts;
}

static void output_encoded_packet(void *data, struct encoder_packet *packet)
{
	(void)data;
	(void)packet;
}

/* Starts ffmpeg_opus at `kbps` on a mix with `speakers`; returns whether
 * the encoder initialized, and its last error in `error`. */
static bool start_opus(enum speaker_layout speakers, int kbps, char *error, size_t error_size)
{
	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = speakers};
	if (!obs_reset_audio(&oai)) {
		fprintf(stderr, "obs_reset_audio failed\n");
		fail();
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "bitrate", kbps);
	obs_encoder_t *encoder = obs_audio_encoder_create("ffmpeg_opus", "opus", settings, 0, NULL);
	obs_data_release(settings);
	assert_non_null(encoder);
	obs_encoder_set_audio(encoder, obs_get_audio());

	obs_output_t *output = obs_output_create(OUTPUT_ID, "output", NULL, NULL);
	assert_non_null(output);
	obs_output_set_audio_encoder(output, encoder, 0);

	bool started = obs_output_initialize_encoders(output, 0);
	const char *last = obs_encoder_get_last_error(encoder);
	snprintf(error, error_size, "%s", last ? last : "");

	obs_output_release(output);
	obs_encoder_release(encoder);
	return started;
}

static void mono_above_256_kbps_starts(void **state)
{
	(void)state;
	if (!module_up)
		skip();

	char error[256];
	if (!start_opus(SPEAKERS_MONO, 320, error, sizeof(error))) {
		fprintf(stderr, "mono Opus at 320 kbps did not start: %s\n", error);
		fail();
	}
}

static void mono_at_256_kbps_starts(void **state)
{
	(void)state;
	if (!module_up)
		skip();

	char error[256];
	if (!start_opus(SPEAKERS_MONO, 256, error, sizeof(error))) {
		fprintf(stderr, "mono Opus at 256 kbps did not start: %s\n", error);
		fail();
	}
}

static void stereo_above_256_kbps_starts(void **state)
{
	(void)state;
	if (!module_up)
		skip();

	char error[256];
	if (!start_opus(SPEAKERS_STEREO, 512, error, sizeof(error))) {
		fprintf(stderr, "stereo Opus at 512 kbps did not start: %s\n", error);
		fail();
	}
}

static int setup(void **state)
{
	(void)state;
	base_set_log_handler(quiet_log, NULL);
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		return 0;
	}

	struct obs_output_info info = {
		.id = OUTPUT_ID,
		.flags = OBS_OUTPUT_AUDIO | OBS_OUTPUT_ENCODED,
		.get_name = output_name,
		.create = output_create,
		.destroy = output_destroy,
		.start = output_start,
		.stop = output_stop,
		.encoded_packet = output_encoded_packet,
		.encoded_audio_codecs = "opus",
	};
	obs_register_output(&info);

	obs_module_t *module = NULL;
	if (obs_open_module(&module, FFMPEG_MODULE_PATH, FFMPEG_DATA_PATH) != MODULE_SUCCESS ||
	    !obs_init_module(module)) {
		fprintf(stderr, "skipped: could not load obs-ffmpeg\n");
		return 0;
	}
	module_up = true;
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(mono_above_256_kbps_starts),
		cmocka_unit_test(mono_at_256_kbps_starts),
		cmocka_unit_test(stereo_above_256_kbps_starts),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
