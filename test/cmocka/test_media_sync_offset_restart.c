/*
 * A Media Source's audio sync offset must not get lost when the source
 * restarts (upstream obsproject/obs-studio#11640, reported by Penwy): with
 * "Restart playback when source becomes active", the delay set in Advanced
 * Audio Properties was applied when the scene was first shown, but after
 * switching to another scene and back the audio played without it.
 *
 * Starting playback shows the preloaded first frame, and libobs then empties
 * the source's audio buffer so the audio continues from that frame. The
 * buffer was made to start at the current system time, without the sync
 * offset, and every packet that continued seamlessly was appended to it, so
 * the offset was gone. A timestamp jump resets the buffer the same way.
 * Whether a packet continues seamlessly depends on the media timeline: in
 * the report it did after the round trip; here it does when the first Media
 * Source of the process starts.
 *
 * The first cases play a generated clip (a flash in the picture and a click
 * in the sound at the same moment) through the real ffmpeg_source with a
 * sync offset, and measure how far the click on the program audio lags the
 * flash on the program video, by their timestamps, as a recording would mux
 * them: when the scene is first shown and again after the program has been
 * on a second scene (with another media source, as in the report) and come
 * back. The last cases drive the two buffer resets directly and check where
 * the buffer starts. All of them need core video, so they run under
 * xvfb-run and skip without a display.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>

#include <obs.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/util_uint64.h>

/* obs-ffmpeg-source.c is compiled into this test; it only needs strings. */
const char *obs_module_text(const char *val)
{
	return val;
}

extern struct obs_source_info ffmpeg_source;

#define W 64
#define H 64
#define RATE 48000
#define MSEC 1000000LL

/* One second at 30 fps: grey, with one bright frame at 300 ms, and silence
 * with a 20 ms click at the same moment. */
#define CLIP_MS 1000
#define FPS 30
#define EVENT_MS 300
#define CLICK_MS 20
#define Y_GREY 90
#define Y_FLASH 230
#define FLASH_LUMA 170
#define CLICK_LEVEL 0.4f
#define AUDIO_PACKET 1024

#define OFFSET_MS 300
/* Without an offset the click reaches the program 50-80 ms before the
 * flash (the same on master): a flash shows on the first output frame at or
 * after its time, a couple of 30 fps frames down the pipeline. Allow for that
 * and for the media thread not delivering exactly on time. */
#define VIDEO_LATE_MS 120
#define VIDEO_EARLY_MS 60

static bool obs_up;
static char clip_path[512];

static bool write_packet(AVFormatContext *oc, int stream, int64_t pts, int64_t duration, const uint8_t *data,
			 size_t size)
{
	AVPacket *pkt = av_packet_alloc();
	if (!pkt || av_new_packet(pkt, (int)size) < 0) {
		av_packet_free(&pkt);
		return false;
	}
	memcpy(pkt->data, data, size);
	pkt->stream_index = stream;
	pkt->pts = pkt->dts = pts;
	pkt->duration = duration;
	pkt->flags |= AV_PKT_FLAG_KEY;
	bool ok = av_interleaved_write_frame(oc, pkt) >= 0;
	av_packet_free(&pkt);
	return ok;
}

/* Raw YUV 4:2:0 video and 16-bit PCM in NUT, written with libavformat so no
 * encoder is needed. */
static bool write_clip(const char *path)
{
	AVFormatContext *oc = NULL;
	if (avformat_alloc_output_context2(&oc, NULL, "nut", path) < 0)
		return false;

	AVStream *vs = avformat_new_stream(oc, NULL);
	AVStream *as = avformat_new_stream(oc, NULL);
	if (!vs || !as)
		goto fail;

	vs->time_base = (AVRational){1, FPS};
	vs->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	vs->codecpar->codec_id = AV_CODEC_ID_RAWVIDEO;
	vs->codecpar->format = AV_PIX_FMT_YUV420P;
	/* NUT stores the raw pixel format as a tag */
	vs->codecpar->codec_tag = avcodec_pix_fmt_to_codec_tag(AV_PIX_FMT_YUV420P);
	vs->codecpar->width = W;
	vs->codecpar->height = H;

	as->time_base = (AVRational){1, RATE};
	as->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
	as->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;
	as->codecpar->format = AV_SAMPLE_FMT_S16;
	as->codecpar->sample_rate = RATE;
	av_channel_layout_default(&as->codecpar->ch_layout, 1);

	if (avio_open(&oc->pb, path, AVIO_FLAG_WRITE) < 0)
		goto fail;
	if (avformat_write_header(oc, NULL) < 0)
		goto fail_io;

	static uint8_t frame[W * H * 3 / 2];
	static int16_t samples[AUDIO_PACKET];
	const int frames = CLIP_MS * FPS / 1000;
	const int64_t total_samples = (int64_t)RATE * CLIP_MS / 1000;
	const int64_t click_start = (int64_t)RATE * EVENT_MS / 1000;
	const int64_t click_end = click_start + RATE * CLICK_MS / 1000;
	int v = 0;
	int64_t a = 0;

	/* interleave by time */
	while (v < frames || a < total_samples) {
		int64_t v_ms = (int64_t)v * 1000 / FPS;
		int64_t a_ms = a * 1000 / RATE;
		if (v < frames && (a >= total_samples || v_ms <= a_ms)) {
			bool flash = v_ms == EVENT_MS;
			memset(frame, flash ? Y_FLASH : Y_GREY, W * H);
			memset(frame + W * H, 128, W * H / 2);
			if (!write_packet(oc, vs->index, av_rescale_q(v, (AVRational){1, FPS}, vs->time_base), 1, frame,
					  sizeof(frame)))
				goto fail_io;
			v++;
		} else {
			int64_t n = total_samples - a < AUDIO_PACKET ? total_samples - a : AUDIO_PACKET;
			for (int64_t i = 0; i < n; i++) {
				int64_t s = a + i;
				samples[i] = (s >= click_start && s < click_end) ? ((s & 8) ? 26000 : -26000) : 0;
			}
			if (!write_packet(oc, as->index, av_rescale_q(a, (AVRational){1, RATE}, as->time_base), n,
					  (const uint8_t *)samples, (size_t)n * sizeof(int16_t)))
				goto fail_io;
			a += n;
		}
	}

	av_write_trailer(oc);
	avio_closep(&oc->pb);
	avformat_free_context(oc);
	return true;

fail_io:
	avio_closep(&oc->pb);
fail:
	avformat_free_context(oc);
	return false;
}

static int group_setup(void **state)
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
		.base_width = W,
		.base_height = H,
		.output_width = W,
		.output_height = H,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) {
		obs_shutdown();
		return 0;
	}

	struct obs_audio_info oai = {.samples_per_sec = RATE, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai)) {
		obs_shutdown();
		return 0;
	}

	obs_register_source(&ffmpeg_source);

	snprintf(clip_path, sizeof(clip_path), "%s/obs-media-sync-offset-%llu.nut",
		 getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (unsigned long long)os_gettime_ns());
	if (!write_clip(clip_path)) {
		fprintf(stderr, "could not write the test clip %s\n", clip_path);
		obs_shutdown();
		return -1;
	}

	obs_up = true;
	return 0;
}

static int group_teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_up) {
		obs_shutdown();
		os_unlink(clip_path);
		obs_up = false;
	}
	return 0;
}

/* Timestamps of the first flash and the first click on the program. */
struct events {
	pthread_mutex_t mutex;
	bool armed;
	/* a grey frame of the new playback was shown, so a bright frame after
	 * it is the flash and not a frame left from before */
	bool grey;
	uint64_t flash;
	uint64_t click;
};

static void raw_video(void *param, struct video_data *frame)
{
	struct events *e = param;
	uint8_t luma = frame->data[0][(H / 2) * frame->linesize[0] + W / 2];

	pthread_mutex_lock(&e->mutex);
	if (e->armed && !e->flash) {
		if (luma <= FLASH_LUMA)
			e->grey = true;
		else if (e->grey)
			e->flash = frame->timestamp;
	}
	pthread_mutex_unlock(&e->mutex);
}

static void raw_audio(void *param, size_t mix_idx, struct audio_data *data)
{
	UNUSED_PARAMETER(mix_idx);
	struct events *e = param;

	pthread_mutex_lock(&e->mutex);
	if (e->armed && !e->click) {
		const float *left = (const float *)data->data[0];
		for (uint32_t i = 0; i < data->frames; i++) {
			if (left[i] > CLICK_LEVEL || left[i] < -CLICK_LEVEL) {
				e->click = data->timestamp + util_mul_div64(i, 1000000000ULL, RATE);
				break;
			}
		}
	}
	pthread_mutex_unlock(&e->mutex);
}

/* Shows the scene and returns how far the click lags the flash, or false if
 * either was missed. */
static bool measure(struct events *e, obs_scene_t *scene, int64_t *delay_ms)
{
	pthread_mutex_lock(&e->mutex);
	e->armed = true;
	e->grey = false;
	e->flash = e->click = 0;
	pthread_mutex_unlock(&e->mutex);

	obs_set_output_source(0, obs_scene_get_source(scene));

	bool seen = false;
	uint64_t flash = 0, click = 0;
	for (int i = 0; i < 400 && !seen; i++) {
		os_sleep_ms(10);
		pthread_mutex_lock(&e->mutex);
		flash = e->flash;
		click = e->click;
		seen = flash && click;
		pthread_mutex_unlock(&e->mutex);
	}

	pthread_mutex_lock(&e->mutex);
	e->armed = false;
	pthread_mutex_unlock(&e->mutex);

	if (!seen) {
		fprintf(stderr, "flash seen: %d, click heard: %d\n", flash != 0, click != 0);
		return false;
	}
	*delay_ms = ((int64_t)click - (int64_t)flash) / MSEC;
	return true;
}

static obs_source_t *create_media(const char *name, bool looping)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", clip_path);
	obs_data_set_bool(settings, "looping", looping);
	obs_data_set_bool(settings, "restart_on_activate", true);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "hw_decode", false);
	obs_source_t *source = obs_source_create("ffmpeg_source", name, settings, NULL);
	obs_data_release(settings);
	return source;
}

static bool in_sync(int64_t delay_ms, int64_t offset_ms)
{
	return delay_ms >= offset_ms - VIDEO_LATE_MS && delay_ms <= offset_ms + VIDEO_EARLY_MS;
}

struct trip {
	int64_t offset_ms;
	bool looping;
	/* how long the scene stays on before switching away, and how long
	 * the program stays on the second scene */
	uint32_t shown_ms;
	uint32_t away_ms;
};

static void round_trip(const struct trip *t)
{
	obs_source_t *media = create_media("delayed media", t->looping);
	obs_source_t *other = create_media("other media", t->looping);
	if (!media || !other) {
		fprintf(stderr, "could not create the media sources\n");
		fail();
	}
	obs_source_set_sync_offset(media, t->offset_ms * MSEC);
	/* only the delayed source is heard */
	obs_source_set_audio_mixers(other, 0);

	obs_scene_t *first = obs_scene_create("first");
	obs_scene_t *second = obs_scene_create("second");
	obs_scene_add(first, media);
	obs_scene_add(second, other);

	struct events e = {0};
	pthread_mutex_init(&e.mutex, NULL);
	obs_add_raw_video_callback(NULL, raw_video, &e);
	obs_add_raw_audio_callback(0, NULL, raw_audio, &e);

	int64_t before = 0, after = 0;
	uint64_t shown_at = os_gettime_ns();
	bool seen_before = measure(&e, first, &before);
	uint64_t shown_ms = (os_gettime_ns() - shown_at) / MSEC;
	if (shown_ms < t->shown_ms)
		os_sleep_ms((uint32_t)(t->shown_ms - shown_ms));

	obs_set_output_source(0, obs_scene_get_source(second));
	os_sleep_ms(t->away_ms);

	bool seen_after = measure(&e, first, &after);

	obs_remove_raw_audio_callback(0, raw_audio, &e);
	obs_remove_raw_video_callback(raw_video, &e);
	obs_set_output_source(0, NULL);
	/* the main canvas holds a reference to its scenes until they are removed */
	obs_source_remove(obs_scene_get_source(first));
	obs_source_remove(obs_scene_get_source(second));
	obs_scene_release(first);
	obs_scene_release(second);
	obs_source_release(media);
	obs_source_release(other);
	pthread_mutex_destroy(&e.mutex);

	fprintf(stderr,
		"%s clip, sync offset %lld ms, shown %u ms, away %u ms: audio %lld ms behind video before the "
		"round trip, %lld ms after\n",
		t->looping ? "looping" : "ended", (long long)t->offset_ms, t->shown_ms, t->away_ms, (long long)before,
		(long long)after);

	if (!seen_before || !seen_after) {
		fprintf(stderr, "the flash or the click was missed %s the round trip\n",
			seen_before ? "after" : "before");
		fail();
	}
	if (!in_sync(before, t->offset_ms)) {
		fprintf(stderr, "the sync offset was not applied when the scene was first shown\n");
		fail();
	}
	if (!in_sync(after, t->offset_ms)) {
		fprintf(stderr, "the sync offset was lost after the scene round trip (%lld ms instead of %lld ms)\n",
			(long long)after, (long long)t->offset_ms);
		fail();
	}
}

/* The report: a looping clip, switched away from and back to. */
static void looping_clip_keeps_offset(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	round_trip(&(struct trip){.offset_ms = OFFSET_MS, .looping = true, .shown_ms = 1500, .away_ms = 500});
}

/* Away for longer than MAX_TS_VAR (2 s). */
static void looping_clip_long_away_keeps_offset(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	round_trip(&(struct trip){.offset_ms = OFFSET_MS, .looping = true, .shown_ms = 1500, .away_ms = 2500});
}

/* The clip plays to its end while the scene is shown. */
static void ended_clip_keeps_offset(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	round_trip(&(struct trip){.offset_ms = OFFSET_MS, .looping = false, .shown_ms = 2500, .away_ms = 500});
}

/* Guard: without an offset, picture and sound stay together. */
static void no_offset_stays_in_sync(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	round_trip(&(struct trip){.offset_ms = 0, .looping = true, .shown_ms = 1500, .away_ms = 2500});
}

/* ------------------------------------------------------------------------ */
/* The libobs half on its own, without the media thread's timing: a source
 * with a sync offset that is not part of any scene, so the audio thread
 * leaves its buffer alone, gets audio packets with media-style timestamps
 * (not near the system clock). After each reset below, the packets that
 * follow are appended to the buffer, which must start at the system time
 * they continue at plus the sync offset. */

static const char *av_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "audio/video source";
}

static void *av_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	return source;
}

static void av_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_source_info av_source = {
	.id = "sync_offset_av_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO,
	.get_name = av_name,
	.create = av_create,
	.destroy = av_destroy,
};

#define PACKET_FRAMES 1024
/* a media timeline position, far from the system clock */
#define MEDIA_TS (5000 * MSEC)

static void output_packet(obs_source_t *source, uint64_t ts)
{
	static float silence[PACKET_FRAMES];
	struct obs_source_audio audio = {
		.data = {(const uint8_t *)silence},
		.frames = PACKET_FRAMES,
		.speakers = SPEAKERS_MONO,
		.format = AUDIO_FORMAT_FLOAT,
		.samples_per_sec = RATE,
		.timestamp = ts,
	};
	obs_source_output_audio(source, &audio);
}

static uint64_t packet_ns(int n)
{
	return util_mul_div64((uint64_t)n * PACKET_FRAMES, 1000000000ULL, RATE);
}

static obs_source_t *create_av_source(void)
{
	obs_register_source(&av_source);
	obs_source_t *source = obs_source_create("sync_offset_av_source", "av", NULL, NULL);
	if (!source) {
		fprintf(stderr, "could not create the source\n");
		fail();
	}
	obs_source_set_sync_offset(source, OFFSET_MS * MSEC);
	return source;
}

/* Where the buffer starts relative to the system time of the reset, which
 * happened between from and to. */
static void check_buffer_start(obs_source_t *source, const char *what, uint64_t from, uint64_t to)
{
	uint64_t audio_ts = obs_source_get_audio_timestamp(source);
	int64_t early_ms = ((int64_t)audio_ts - (int64_t)to) / MSEC;
	int64_t late_ms = ((int64_t)audio_ts - (int64_t)from) / MSEC;

	fprintf(stderr, "%s: buffer starts %lld-%lld ms after the reset (sync offset %d ms)\n", what,
		(long long)early_ms, (long long)late_ms, OFFSET_MS);
	if (!audio_ts || late_ms < OFFSET_MS - 5 || early_ms > OFFSET_MS + 5) {
		fprintf(stderr, "%s: the packets after the reset lost the sync offset\n", what);
		fail();
	}
}

/* The Media Source restart: playback starts over with the first frame
 * preloaded, shown at once, and audio continues from that frame. */
static void preloaded_frame_keeps_offset(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();

	obs_source_t *source = create_av_source();

	output_packet(source, MEDIA_TS);

	static uint8_t pixels[W * H * 4];
	struct obs_source_frame frame = {
		.data = {pixels},
		.linesize = {W * 4},
		.width = W,
		.height = H,
		.format = VIDEO_FORMAT_BGRA,
		.full_range = true,
		.timestamp = MEDIA_TS + packet_ns(1),
	};
	obs_source_preload_video(source, &frame);

	uint64_t from = os_gettime_ns();
	obs_source_show_preloaded_video(source);
	uint64_t to = os_gettime_ns();

	output_packet(source, MEDIA_TS + packet_ns(1));
	output_packet(source, MEDIA_TS + packet_ns(2));
	check_buffer_start(source, "preloaded frame shown", from, to);

	obs_source_release(source);
}

/* The timestamps jump by more than MAX_TS_VAR (2 s). */
static void timestamp_jump_keeps_offset(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();

	obs_source_t *source = create_av_source();

	output_packet(source, MEDIA_TS);
	output_packet(source, MEDIA_TS + packet_ns(1));

	uint64_t from = os_gettime_ns();
	output_packet(source, MEDIA_TS + 10000 * MSEC);
	uint64_t to = os_gettime_ns();
	output_packet(source, MEDIA_TS + 10000 * MSEC + packet_ns(1));
	check_buffer_start(source, "timestamp jump", from, to);

	obs_source_release(source);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(looping_clip_keeps_offset),    cmocka_unit_test(looping_clip_long_away_keeps_offset),
		cmocka_unit_test(ended_clip_keeps_offset),      cmocka_unit_test(no_offset_stays_in_sync),
		cmocka_unit_test(preloaded_frame_keeps_offset), cmocka_unit_test(timestamp_jump_keeps_offset),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
