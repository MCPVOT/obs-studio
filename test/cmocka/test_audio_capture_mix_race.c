/* #130: real capture packets race with the real mixer. Run with libobs and
 * this executable built with -fsanitize=thread to detect unlocked timestamp
 * and deque reads. Events pace capture from mixer callbacks; no sleeps or
 * unsynchronized test state are used. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <obs.h>
#include <util/platform.h>
#include <util/threading.h>

#define RATE 48000
#define FRAMES 1024
#define TICKS 60
#define PACKETS_PER_TICK 100

struct capture {
	obs_source_t *source;
	os_sem_t *tick;
	bool failed;
	size_t packets;
};

static const char *capture_name(void *unused)
{
	(void)unused;
	return "concurrent capture";
}

static void *capture_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void capture_destroy(void *data)
{
	(void)data;
}

static void quiet_log(int level, const char *format, va_list args, void *param)
{
	(void)level;
	(void)format;
	(void)args;
	(void)param;
}

static void mixed_audio(void *param, size_t mix, struct audio_data *audio)
{
	(void)mix;
	(void)audio;
	struct capture *capture = param;
	os_sem_post(capture->tick);
}

static void *produce_audio(void *param)
{
	struct capture *capture = param;
	float samples[FRAMES];
	for (size_t i = 0; i < FRAMES; i++)
		samples[i] = 0.25f;
	struct obs_source_audio audio = {
		.data = {(const uint8_t *)samples, (const uint8_t *)samples},
		.frames = FRAMES,
		.speakers = SPEAKERS_STEREO,
		.format = AUDIO_FORMAT_FLOAT_PLANAR,
		.samples_per_sec = RATE,
	};
	for (size_t tick = 0; tick < TICKS; tick++) {
		if (os_sem_wait(capture->tick) != 0) {
			capture->failed = true;
			break;
		}
		for (size_t packet = 0; packet < PACKETS_PER_TICK; packet++) {
			/* Discontinuities exercise reset_audio_data as well as deque
			 * growth while the mixer renders and checks buffering. */
			audio.timestamp = os_gettime_ns() + (packet & 1 ? 200000000ULL : 0);
			obs_source_output_audio(capture->source, &audio);
			capture->packets++;
		}
	}
	return NULL;
}

static void capture_and_mix(void **state)
{
	(void)state;
	base_set_log_handler(quiet_log, NULL);
	if (!obs_startup("en-US", NULL, NULL))
		skip();
	struct obs_video_info video = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = 64,
		.base_height = 64,
		.output_width = 64,
		.output_height = 64,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&video) != OBS_VIDEO_SUCCESS) {
		obs_shutdown();
		skip();
	}
	struct obs_audio_info audio = {.samples_per_sec = RATE, .speakers = SPEAKERS_STEREO};
	assert_true(obs_reset_audio(&audio));
	struct obs_source_info info = {
		.id = "test_audio_capture_mix_race",
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_AUDIO,
		.get_name = capture_name,
		.create = capture_create,
		.destroy = capture_destroy,
	};
	obs_register_source(&info);
	struct capture capture = {0};
	assert_int_equal(os_sem_init(&capture.tick, 0), 0);
	capture.source = obs_source_create_private(info.id, "concurrent capture", NULL);
	assert_non_null(capture.source);
	obs_set_output_source(0, capture.source);
	obs_add_raw_audio_callback(0, NULL, mixed_audio, &capture);
	pthread_t producer;
	assert_int_equal(pthread_create(&producer, NULL, produce_audio, &capture), 0);
	assert_int_equal(pthread_join(producer, NULL), 0);
	obs_remove_raw_audio_callback(0, mixed_audio, &capture);
	obs_set_output_source(0, NULL);
	obs_source_release(capture.source);
	obs_shutdown();
	os_sem_destroy(capture.tick);
	assert_false(capture.failed);
	assert_int_equal(capture.packets, TICKS * PACKETS_PER_TICK);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(capture_and_mix)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
