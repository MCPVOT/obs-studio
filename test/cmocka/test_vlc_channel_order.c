/*
 * VLC Video Source must deliver surround channels in the order libobs
 * expects (upstream obsproject/obs-studio#12579).
 *
 * libVLC's audio callbacks hand over interleaved samples in VLC's own channel
 * order (L R [SL SR] RL RR C LFE), but libobs reads 5.1 as FL FR FC LFE RL RR
 * and 7.1 as FL FR FC LFE RL RR SL SR. Without a reorder the centre and LFE
 * play from the rear speakers and the other way round.
 *
 * The unit tests check the reorder helper directly. The end-to-end tests
 * play generated 5.1 (back and side) and 7.1 WAV files (each channel at its own level)
 * through the real vlc_source module and measure the level of every channel
 * libobs receives. They skip without a display or when libVLC is not
 * installed; the test then exits with 77 so ctest reports it as skipped.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

#include "vlc-audio-order.h"

/* ------------------------------------------------------------------------ */
/* Unit tests of the reorder helper                                          */

static void check_reorder(unsigned channels, size_t sample_size, const int *vlc_order_to_obs)
{
	/* vlc_order_to_obs[i]: OBS channel index that VLC channel i carries */
	const unsigned frames = 3;
	uint8_t buf[3 * 8 * 4];
	memset(buf, 0, sizeof(buf));

	/* Fill every VLC-ordered sample with the OBS channel number it belongs
	 * to (plus a per-frame offset) so the expected output is simply
	 * "OBS channel c holds value c". */
	for (unsigned f = 0; f < frames; f++) {
		for (unsigned i = 0; i < channels; i++) {
			uint8_t *s = buf + (f * channels + i) * sample_size;
			uint32_t v = (uint32_t)(vlc_order_to_obs[i] + 10 * f);
			memcpy(s, &v, sample_size < 4 ? sample_size : 4);
		}
	}

	vlc_audio_reorder(buf, frames, channels, sample_size);

	for (unsigned f = 0; f < frames; f++) {
		for (unsigned c = 0; c < channels; c++) {
			uint32_t v = 0;
			memcpy(&v, buf + (f * channels + c) * sample_size, sample_size < 4 ? sample_size : 4);
			if (v != c + 10 * f) {
				fprintf(stderr,
					"%u channels, %zu-byte samples: frame %u OBS channel %u holds VLC "
					"data for OBS channel %u\n",
					channels, sample_size, f, c, v - 10 * f);
				fail();
			}
		}
	}
}

/* VLC 5.1: L R RL RR C LFE  ->  OBS 5.1: FL FR FC LFE RL RR */
static const int vlc51[6] = {0, 1, 4, 5, 2, 3};
/* VLC 7.1: L R SL SR RL RR C LFE  ->  OBS 7.1: FL FR FC LFE RL RR SL SR */
static const int vlc71[8] = {0, 1, 6, 7, 4, 5, 2, 3};

static void reorder_5_1(void **state)
{
	UNUSED_PARAMETER(state);
	check_reorder(6, 2, vlc51);
	check_reorder(6, 4, vlc51);
}

static void reorder_7_1(void **state)
{
	UNUSED_PARAMETER(state);
	check_reorder(8, 2, vlc71);
	check_reorder(8, 4, vlc71);
}

static void reorder_leaves_other_layouts(void **state)
{
	UNUSED_PARAMETER(state);
	static const int same[5] = {0, 1, 2, 3, 4};
	for (unsigned channels = 1; channels <= 5; channels++) {
		check_reorder(channels, 2, same);
		check_reorder(channels, 4, same);
	}
}

/* ------------------------------------------------------------------------ */
/* End to end through vlc_source                                             */

#define RATE 48000
#define FILE_SECONDS 3

/* Level written into each channel of the file, in WAV/OBS channel order. */
static float level_of(unsigned ch)
{
	return 0.05f * (float)(ch + 1);
}

static void put_u16(FILE *f, uint16_t v)
{
	fwrite(&v, 2, 1, f);
}

static void put_u32(FILE *f, uint32_t v)
{
	fwrite(&v, 4, 1, f);
}

/* WAVE_FORMAT_EXTENSIBLE float file; channel mask gives the standard WAV
 * order, which is the libobs order for 5.1 (back) and 7.1. */
static bool write_wav(const char *path, unsigned channels, uint32_t mask)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;

	const uint32_t frames = RATE * FILE_SECONDS;
	const uint32_t data_size = frames * channels * 4;
	static const uint8_t float_guid[16] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
					       0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

	fwrite("RIFF", 1, 4, f);
	put_u32(f, 4 + 8 + 40 + 8 + data_size);
	fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f);
	put_u32(f, 40);
	put_u16(f, 0xFFFE);
	put_u16(f, (uint16_t)channels);
	put_u32(f, RATE);
	put_u32(f, RATE * channels * 4);
	put_u16(f, (uint16_t)(channels * 4));
	put_u16(f, 32);
	put_u16(f, 22);
	put_u16(f, 32);
	put_u32(f, mask);
	fwrite(float_guid, 1, 16, f);
	fwrite("data", 1, 4, f);
	put_u32(f, data_size);

	for (uint32_t i = 0; i < frames; i++) {
		float s = sinf(2.0f * 3.14159265f * 400.0f * (float)i / RATE);
		for (unsigned ch = 0; ch < channels; ch++) {
			float v = s * level_of(ch);
			fwrite(&v, 4, 1, f);
		}
	}

	fclose(f);
	return true;
}

struct capture {
	pthread_mutex_t mutex;
	unsigned channels;
	uint64_t frames;
	double sumsq[MAX_AUDIO_CHANNELS];
};

static void audio_cb(void *param, obs_source_t *source, const struct audio_data *audio, bool muted)
{
	struct capture *cap = param;
	UNUSED_PARAMETER(source);
	UNUSED_PARAMETER(muted);

	pthread_mutex_lock(&cap->mutex);
	for (unsigned ch = 0; ch < cap->channels; ch++) {
		const float *d = (const float *)audio->data[ch];
		if (!d)
			continue;
		for (uint32_t i = 0; i < audio->frames; i++)
			cap->sumsq[ch] += (double)d[i] * d[i];
	}
	cap->frames += audio->frames;
	pthread_mutex_unlock(&cap->mutex);
}

/* One libobs instance (7.1 audio, so neither file is downmixed) for all
 * end-to-end cases. vlc_source applies its settings from the video thread,
 * so core video is needed too. */
static bool obs_ready;

static int start_obs(void **state)
{
	UNUSED_PARAMETER(state);
	const char *display = getenv("DISPLAY");
	if (!display || !*display)
		return 0;

	if (!obs_startup("en-US", NULL, NULL))
		return 0;

	struct obs_video_info ovi = {
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
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return 0;

	struct obs_audio_info oai = {.samples_per_sec = RATE, .speakers = SPEAKERS_7POINT1};
	if (!obs_reset_audio(&oai))
		return 0;

	obs_module_t *mod = NULL;
	if (obs_open_module(&mod, VLC_MODULE_PATH, VLC_DATA_PATH) != MODULE_SUCCESS || !obs_init_module(mod))
		return 0;

	/* the module only registers vlc_source when libVLC is installed */
	obs_ready = obs_source_get_display_name("vlc_source") != NULL;
	return 0;
}

static int stop_obs(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_initialized())
		obs_shutdown();
	return 0;
}

static void play_and_check(unsigned channels, uint32_t mask)
{
	if (!obs_ready)
		skip(); /* no display, or libVLC is not installed */

	char path[512];
	snprintf(path, sizeof(path), "%s/obs-vlc-order-%llu-%u.wav", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp",
		 (unsigned long long)os_gettime_ns(), channels);
	if (!write_wav(path, channels, mask)) {
		fprintf(stderr, "could not write %s\n", path);
		fail();
	}

	obs_data_t *settings = obs_data_create();
	obs_data_array_t *list = obs_data_array_create();
	obs_data_t *item = obs_data_create();
	obs_data_set_string(item, "value", path);
	obs_data_array_push_back(list, item);
	obs_data_set_array(settings, "playlist", list);
	obs_data_set_string(settings, "playback_behavior", "always_play");
	obs_data_release(item);
	obs_data_array_release(list);

	struct capture cap = {.channels = channels};
	pthread_mutex_init(&cap.mutex, NULL);

	obs_source_t *src = obs_source_create("vlc_source", "vlc", settings, NULL);
	obs_data_release(settings);
	if (!src) {
		os_unlink(path);
		fprintf(stderr, "could not create vlc_source\n");
		fail();
	}
	obs_source_add_audio_capture_callback(src, audio_cb, &cap);

	/* Collect one second of audio (VLC may stop when the file ends). */
	uint64_t frames = 0;
	for (int i = 0; i < 1000 && frames < RATE; i++) {
		os_sleep_ms(10);
		pthread_mutex_lock(&cap.mutex);
		frames = cap.frames;
		pthread_mutex_unlock(&cap.mutex);
	}

	obs_source_remove_audio_capture_callback(src, audio_cb, &cap);
	obs_source_release(src);
	os_unlink(path);

	if (frames < RATE / 2) {
		fprintf(stderr, "vlc_source delivered only %llu frames\n", (unsigned long long)frames);
		fail();
	}

	/* A sine of peak p has RMS p / sqrt(2). Identify which file channel
	 * each libobs channel carries by its level. */
	bool ok = true;
	for (unsigned ch = 0; ch < channels; ch++) {
		double rms = sqrt(cap.sumsq[ch] / (double)frames) * sqrt(2.0);
		int got = (int)lround(rms / 0.05) - 1;
		fprintf(stderr, "%u ch: libobs channel %u carries file channel %d (peak %.3f)\n", channels, ch, got,
			rms);
		if (got != (int)ch)
			ok = false;
	}
	pthread_mutex_destroy(&cap.mutex);

	if (!ok) {
		fprintf(stderr, "%u-channel file: channels arrive in the wrong order\n", channels);
		fail();
	}
}

static void vlc_source_5_1(void **state)
{
	UNUSED_PARAMETER(state);
	/* FL FR FC LFE BL BR */
	play_and_check(6, 0x3F);
}

static void vlc_source_5_1_side(void **state)
{
	UNUSED_PARAMETER(state);
	/* FL FR FC LFE SL SR: VLC plays the side pair as its 5.1 surrounds,
	 * which libobs 5.1 carries as RL RR */
	play_and_check(6, 0x60F);
}

static void vlc_source_7_1(void **state)
{
	UNUSED_PARAMETER(state);
	/* FL FR FC LFE BL BR SL SR */
	play_and_check(8, 0x63F);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(reorder_5_1),
		cmocka_unit_test(reorder_7_1),
		cmocka_unit_test(reorder_leaves_other_layouts),
		cmocka_unit_test(vlc_source_5_1),
		cmocka_unit_test(vlc_source_5_1_side),
		cmocka_unit_test(vlc_source_7_1),
	};

	int failed = cmocka_run_group_tests(tests, start_obs, stop_obs);
	if (failed)
		return failed;
	return obs_ready ? 0 : 77;
}
