/*
 * Scene transitions must fade audio while an encoder rescales the output
 * (upstream obsproject/obs-studio#12912).
 *
 * An encoder with a GPU scaled size (Settings > Output > Rescale Output)
 * gets an encoder-only video mix that shares the main mix's view. The audio
 * thread builds its render tree once per mix, so it walks the same view
 * twice, tags every source under the transition as a duplicate and mixes it
 * on its own at full volume instead of through the transition's fade.
 *
 * The test fades from an empty scene to a scene with a constant-level audio
 * source over two seconds and measures the program mix (track 1) during the
 * first part of the fade, once without and once with a rescaling encoder.
 * It needs core video, so it runs under xvfb-run and skips without a
 * display.
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

#define RATE 48000
#define LEVEL 0.5f
#define FADE_MS 2000

/* ------------------------------------------------------------------------ */
/* Constant-level audio source                                               */

struct tone {
	obs_source_t *source;
	pthread_t thread;
	os_event_t *stop;
};

static void *tone_thread(void *data)
{
	struct tone *t = data;
	static float buf[480];
	for (size_t i = 0; i < 480; i++)
		buf[i] = LEVEL;

	uint64_t next = os_gettime_ns();
	while (os_event_try(t->stop) == EAGAIN) {
		struct obs_source_audio a = {
			.data = {(uint8_t *)buf, (uint8_t *)buf},
			.frames = 480,
			.speakers = SPEAKERS_STEREO,
			.format = AUDIO_FORMAT_FLOAT_PLANAR,
			.samples_per_sec = RATE,
			.timestamp = next,
		};
		obs_source_output_audio(t->source, &a);
		next += 10000000ULL;
		os_sleepto_ns(next);
	}
	return NULL;
}

static const char *tone_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "tone";
}

static void *tone_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	struct tone *t = bzalloc(sizeof(*t));
	t->source = source;
	os_event_init(&t->stop, OS_EVENT_TYPE_MANUAL);
	pthread_create(&t->thread, NULL, tone_thread, t);
	return t;
}

static void tone_destroy(void *data)
{
	struct tone *t = data;
	os_event_signal(t->stop);
	pthread_join(t->thread, NULL);
	os_event_destroy(t->stop);
	bfree(t);
}

static struct obs_source_info tone_info = {
	.id = "test_tone",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = tone_name,
	.create = tone_create,
	.destroy = tone_destroy,
};

/* ------------------------------------------------------------------------ */
/* Linear audio fade transition (the audio half of the Fade transition)      */

static const char *xfade_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "xfade";
}

static void *xfade_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	return source;
}

static void xfade_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static void xfade_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	obs_transition_video_render(data, NULL);
}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

static bool xfade_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers,
			       size_t channels, size_t sample_rate)
{
	return obs_transition_audio_render(data, ts_out, audio, mixers, channels, sample_rate, mix_a, mix_b);
}

static struct obs_source_info xfade_info = {
	.id = "test_xfade",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = xfade_name,
	.create = xfade_create,
	.destroy = xfade_destroy,
	.video_render = xfade_video_render,
	.audio_render = xfade_audio_render,
};

/* ------------------------------------------------------------------------ */
/* Encoder that only needs to exist: initializing it with a scaled size     */
/* creates the encoder-only video mix.                                       */

static const char *venc_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "scaled encoder";
}

static void *venc_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(encoder);
	return bzalloc(1);
}

static void venc_destroy(void *data)
{
	bfree(data);
}

static bool venc_encode(void *data, struct encoder_frame *frame, struct encoder_packet *packet, bool *received_packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(frame);
	UNUSED_PARAMETER(packet);
	*received_packet = false;
	return true;
}

static struct obs_encoder_info venc_info = {
	.id = "test_scaled_venc",
	.type = OBS_ENCODER_VIDEO,
	.codec = "h264",
	.get_name = venc_name,
	.create = venc_create,
	.destroy = venc_destroy,
	.encode = venc_encode,
};

/* Output whose start only initializes its encoder, as real outputs do. */
static const char *out_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "scaled output";
}

static void *out_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	return output;
}

static void out_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static bool out_start(void *data)
{
	return obs_output_initialize_encoders(data, 0);
}

static void out_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);
	obs_output_end_data_capture(data);
}

static void out_packet(void *data, struct encoder_packet *packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(packet);
}

static struct obs_output_info out_info = {
	.id = "test_scaled_output",
	.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED,
	.encoded_video_codecs = "h264",
	.get_name = out_name,
	.create = out_create,
	.destroy = out_destroy,
	.start = out_start,
	.stop = out_stop,
	.encoded_packet = out_packet,
};

/* ------------------------------------------------------------------------ */

struct meter {
	pthread_mutex_t mutex;
	uint64_t start_ts;
	uint64_t end_ts;
	double sum;
	uint64_t frames;
};

static void raw_audio(void *param, size_t mix_idx, struct audio_data *data)
{
	struct meter *m = param;
	UNUSED_PARAMETER(mix_idx);

	pthread_mutex_lock(&m->mutex);
	if (m->start_ts && data->timestamp >= m->start_ts && data->timestamp < m->end_ts) {
		const float *d = (const float *)data->data[0];
		for (uint32_t i = 0; i < data->frames; i++)
			m->sum += fabsf(d[i]);
		m->frames += data->frames;
	}
	pthread_mutex_unlock(&m->mutex);
}

static bool init_core(void)
{
	if (!obs_startup("en-US", NULL, NULL))
		return false;

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
		return false;

	struct obs_audio_info oai = {.samples_per_sec = RATE, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai))
		return false;

	obs_register_source(&tone_info);
	obs_register_source(&xfade_info);
	obs_register_encoder(&venc_info);
	obs_register_output(&out_info);
	return true;
}

/* Average program level over the first quarter of a fade-in. */
static double fade_in_level(bool rescale)
{
	obs_scene_t *empty = obs_scene_create("empty");
	obs_scene_t *with_tone = obs_scene_create("with tone");
	obs_source_t *tone = obs_source_create("test_tone", "tone", NULL, NULL);
	obs_scene_add(with_tone, tone);

	obs_source_t *xfade = obs_source_create_private("test_xfade", "xfade", NULL);
	obs_transition_set(xfade, obs_scene_get_source(empty));
	obs_set_output_source(0, xfade);

	obs_encoder_t *venc = NULL;
	obs_output_t *out = NULL;
	if (rescale) {
		venc = obs_video_encoder_create("test_scaled_venc", "venc", NULL, NULL);
		obs_encoder_set_gpu_scale_type(venc, OBS_SCALE_BICUBIC);
		obs_encoder_set_scaled_size(venc, 32, 32);
		obs_encoder_set_video(venc, obs_get_video());
		out = obs_output_create("test_scaled_output", "out", NULL, NULL);
		obs_output_set_video_encoder(out, venc);
		assert_true(obs_output_start(out));
	}

	struct meter m = {0};
	pthread_mutex_init(&m.mutex, NULL);
	obs_add_raw_audio_callback(0, NULL, raw_audio, &m);

	os_sleep_ms(500);

	uint64_t start = os_gettime_ns();
	pthread_mutex_lock(&m.mutex);
	/* skip the first 100 ms (audio buffering), stop at 25 % of the fade */
	m.start_ts = start + 100000000ULL;
	m.end_ts = start + (uint64_t)FADE_MS * 1000000ULL / 4;
	pthread_mutex_unlock(&m.mutex);

	obs_transition_start(xfade, OBS_TRANSITION_MODE_AUTO, FADE_MS, obs_scene_get_source(with_tone));
	os_sleep_ms(FADE_MS + 1500);

	obs_remove_raw_audio_callback(0, raw_audio, &m);
	obs_set_output_source(0, NULL);
	if (out) {
		obs_output_force_stop(out);
		obs_output_release(out);
	}
	if (venc)
		obs_encoder_release(venc);
	obs_source_release(xfade);
	/* scenes belong to the main canvas until removed */
	obs_source_remove(obs_scene_get_source(with_tone));
	obs_source_remove(obs_scene_get_source(empty));
	obs_scene_release(with_tone);
	obs_scene_release(empty);
	obs_source_remove(tone);
	obs_source_release(tone);

	pthread_mutex_destroy(&m.mutex);
	if (!m.frames) {
		fprintf(stderr, "no program audio during the fade\n");
		fail();
	}
	return m.sum / (double)m.frames / LEVEL;
}

static bool core_ready;

static int setup(void **state)
{
	UNUSED_PARAMETER(state);
	const char *display = getenv("DISPLAY");
	if (display && *display)
		core_ready = init_core();
	return 0;
}

static int teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_initialized())
		obs_shutdown();
	return 0;
}

static void check_fade(bool rescale)
{
	if (!core_ready)
		skip();

	double level = fade_in_level(rescale);

	/* A linear fade-in averages about 0.07-0.15 of full level over 5-25 %
	 * of its duration; mixing the source outside the fade gives ~1. */
	fprintf(stderr, "%s: program level in the first quarter of the fade: %.2f of full\n",
		rescale ? "rescaled output" : "no rescaling", level);
	if (level > 0.3) {
		fprintf(stderr, "the incoming scene's audio is not faded in\n");
		fail();
	}
}

static void fade_without_rescale(void **state)
{
	UNUSED_PARAMETER(state);
	check_fade(false);
}

static void fade_with_rescaled_encoder(void **state)
{
	UNUSED_PARAMETER(state);
	check_fade(true);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(fade_without_rescale),
		cmocka_unit_test(fade_with_rescaled_encoder),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
