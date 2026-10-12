/*
 * A Media Source playing an audio file with embedded cover art must keep
 * playing after a seek (upstream obsproject/obs-studio#11195, reported by
 * ccc-rrr, with #9428 and #11739): clicking the seek bar of an MP3 with an
 * album cover made playback end and start over from the beginning, while
 * the same MP3 without a cover seeked normally.
 *
 * FFmpeg exposes the cover as a video stream with a single packet (an
 * attached picture). After that one frame, the media thread keeps reading
 * ahead for the next video frame, reaches the end of the file at once and
 * records it. A seek moves the demuxer back into the file but kept that end
 * of file, so no further packets were read: the decoders drained, playback
 * ended and the source reset to the start.
 *
 * The cases play generated files (a tone, with and without a cover, as MP3
 * when an MP3 encoder is available and always as FLAC) through the real
 * ffmpeg_source, seek into the middle with obs_source_media_set_time() and
 * check the media state, the media time and how much of the tone reaches
 * the program audio afterwards. Another case shows the scene again after the
 * program was on another scene (restart on activate) and checks the tone
 * comes back. They need core video, so they run under xvfb-run and skip
 * without a display.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>

#include <obs.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

/* obs-ffmpeg-source.c is compiled into this test; it only needs strings. */
const char *obs_module_text(const char *val)
{
	return val;
}

extern struct obs_source_info ffmpeg_source;

#define W 64
#define H 64
#define RATE 48000

/* a 5 second 440 Hz tone */
#define CLIP_MS 5000
#define TONE_HZ 440.0
#define TONE_LEVEL 0.5f
/* a program audio packet with a sample above this carries the tone */
#define HEARD_LEVEL 0.1f

#define SEEK_MS 2500
/* how long playback is watched after the seek, and how much of the tone
 * must reach the program in that time (the rest of the clip is 2500 ms) */
#define WATCH_MS 1500
#define MIN_HEARD_MS 1000

static bool obs_up;
static bool have_mp3;

enum clip { MP3_COVER, MP3_PLAIN, FLAC_COVER, FLAC_PLAIN, CLIP_COUNT };
static const char *const clip_names[CLIP_COUNT] = {"MP3 with cover art", "MP3 without cover art", "FLAC with cover art",
						   "FLAC without cover art"};
static char clip_paths[CLIP_COUNT][512];

/* ------------------------------------------------------------------------ */
/* Test files */

static bool mux_packet(AVFormatContext *oc, AVPacket *pkt, AVStream *st, AVRational enc_tb)
{
	pkt->stream_index = st->index;
	av_packet_rescale_ts(pkt, enc_tb, st->time_base);
	return av_interleaved_write_frame(oc, pkt) >= 0;
}

/* One JPEG frame of a solid colour, as an album cover. */
static bool encode_cover(AVPacket *out, AVCodecParameters *par)
{
	const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
	if (!codec)
		return false;

	AVCodecContext *enc = avcodec_alloc_context3(codec);
	AVFrame *frame = av_frame_alloc();
	bool ok = false;
	if (!enc || !frame)
		goto done;

	enc->width = W;
	enc->height = H;
	enc->pix_fmt = AV_PIX_FMT_YUVJ420P;
	enc->color_range = AVCOL_RANGE_JPEG;
	enc->time_base = (AVRational){1, 25};
	if (avcodec_open2(enc, codec, NULL) < 0)
		goto done;

	frame->width = W;
	frame->height = H;
	frame->format = AV_PIX_FMT_YUVJ420P;
	if (av_frame_get_buffer(frame, 0) < 0)
		goto done;
	for (int y = 0; y < H; y++)
		memset(frame->data[0] + y * frame->linesize[0], 180, W);
	for (int y = 0; y < H / 2; y++) {
		memset(frame->data[1] + y * frame->linesize[1], 60, W / 2);
		memset(frame->data[2] + y * frame->linesize[2], 200, W / 2);
	}
	frame->pts = 0;

	if (avcodec_send_frame(enc, frame) < 0 || avcodec_send_frame(enc, NULL) < 0)
		goto done;
	if (avcodec_receive_packet(enc, out) < 0)
		goto done;
	ok = avcodec_parameters_from_context(par, enc) >= 0;

done:
	av_frame_free(&frame);
	avcodec_free_context(&enc);
	return ok;
}

static void fill_tone(AVFrame *frame, int64_t first)
{
	for (int i = 0; i < frame->nb_samples; i++) {
		float v = TONE_LEVEL * (float)sin(2.0 * M_PI * TONE_HZ * (double)(first + i) / RATE);
		switch (frame->format) {
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP:
			((float *)frame->data[0])[i] = v;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			((int16_t *)frame->data[0])[i] = (int16_t)(v * 32767.0f);
			break;
		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			((int32_t *)frame->data[0])[i] = (int32_t)(v * 2147483647.0f);
			break;
		default:
			break;
		}
	}
}

static enum AVSampleFormat pick_sample_format(const AVCodecContext *enc, const AVCodec *codec)
{
	static const enum AVSampleFormat wanted[] = {AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_FLT,  AV_SAMPLE_FMT_S16P,
						     AV_SAMPLE_FMT_S16,  AV_SAMPLE_FMT_S32P, AV_SAMPLE_FMT_S32};
	const enum AVSampleFormat *formats = NULL;
	int count = 0;
	if (avcodec_get_supported_config(enc, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **)&formats,
					 &count) < 0 ||
	    !formats)
		return AV_SAMPLE_FMT_NONE;

	for (size_t w = 0; w < sizeof(wanted) / sizeof(wanted[0]); w++)
		for (int i = 0; i < count; i++)
			if (formats[i] == wanted[w])
				return wanted[w];
	return AV_SAMPLE_FMT_NONE;
}

static bool drain_audio(AVFormatContext *oc, AVCodecContext *enc, AVStream *st, AVPacket *pkt)
{
	int ret;
	while ((ret = avcodec_receive_packet(enc, pkt)) >= 0) {
		if (!mux_packet(oc, pkt, st, enc->time_base))
			return false;
	}
	return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF;
}

/* A mono tone, encoded with the given codec, with a JPEG cover as an
 * attached picture if asked. The muxers (ID3v2 APIC in MP3, a PICTURE block
 * in FLAC) need the cover before the first audio packet. */
static bool write_clip(const char *path, const char *format, enum AVCodecID codec_id, bool cover)
{
	const AVCodec *codec = avcodec_find_encoder(codec_id);
	if (!codec)
		return false;

	AVFormatContext *oc = NULL;
	AVCodecContext *enc = NULL;
	AVPacket *pkt = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	bool ok = false;

	if (!pkt || !frame || avformat_alloc_output_context2(&oc, NULL, format, path) < 0)
		goto done;

	enc = avcodec_alloc_context3(codec);
	if (!enc)
		goto done;
	enc->sample_rate = RATE;
	enc->time_base = (AVRational){1, RATE};
	av_channel_layout_default(&enc->ch_layout, 1);
	enc->sample_fmt = pick_sample_format(enc, codec);
	if (enc->sample_fmt == AV_SAMPLE_FMT_NONE)
		goto done;
	if (codec_id == AV_CODEC_ID_MP3)
		enc->bit_rate = 128000;
	if (avcodec_open2(enc, codec, NULL) < 0)
		goto done;

	AVStream *as = avformat_new_stream(oc, NULL);
	if (!as || avcodec_parameters_from_context(as->codecpar, enc) < 0)
		goto done;
	as->time_base = enc->time_base;

	AVStream *vs = NULL;
	AVPacket *pic = NULL;
	if (cover) {
		vs = avformat_new_stream(oc, NULL);
		pic = av_packet_alloc();
		if (!vs || !pic || !encode_cover(pic, vs->codecpar)) {
			av_packet_free(&pic);
			goto done;
		}
		vs->disposition |= AV_DISPOSITION_ATTACHED_PIC;
		vs->time_base = (AVRational){1, 90000};
	}

	if (avio_open(&oc->pb, path, AVIO_FLAG_WRITE) < 0) {
		av_packet_free(&pic);
		goto done;
	}
	if (avformat_write_header(oc, NULL) < 0) {
		av_packet_free(&pic);
		goto done_io;
	}

	if (pic) {
		pic->stream_index = vs->index;
		pic->pts = pic->dts = 0;
		pic->flags |= AV_PKT_FLAG_KEY;
		bool written = av_write_frame(oc, pic) >= 0;
		av_packet_free(&pic);
		if (!written)
			goto done_io;
	}

	const int64_t total = (int64_t)RATE * CLIP_MS / 1000;
	const int chunk = enc->frame_size > 0 ? enc->frame_size : 4096;
	for (int64_t done_samples = 0; done_samples < total;) {
		av_frame_unref(frame);
		frame->nb_samples = (int)(total - done_samples < chunk ? total - done_samples : chunk);
		frame->format = enc->sample_fmt;
		frame->sample_rate = RATE;
		if (av_channel_layout_copy(&frame->ch_layout, &enc->ch_layout) < 0 || av_frame_get_buffer(frame, 0) < 0)
			goto done_io;
		fill_tone(frame, done_samples);
		frame->pts = done_samples;
		done_samples += frame->nb_samples;

		if (avcodec_send_frame(enc, frame) < 0 || !drain_audio(oc, enc, as, pkt))
			goto done_io;
	}
	if (avcodec_send_frame(enc, NULL) < 0 || !drain_audio(oc, enc, as, pkt))
		goto done_io;

	ok = av_write_trailer(oc) >= 0;

done_io:
	avio_closep(&oc->pb);
done:
	av_frame_free(&frame);
	av_packet_free(&pkt);
	avcodec_free_context(&enc);
	avformat_free_context(oc);
	return ok;
}

/* ------------------------------------------------------------------------ */

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

	const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
	unsigned long long id = (unsigned long long)os_gettime_ns();
	static const char *const exts[CLIP_COUNT] = {"mp3", "mp3", "flac", "flac"};
	for (int c = 0; c < CLIP_COUNT; c++)
		snprintf(clip_paths[c], sizeof(clip_paths[c]), "%s/obs-media-cover-%llu-%d.%s", tmp, id, c, exts[c]);

	/* MP3 needs an external encoder (LAME); FLAC is built in. */
	have_mp3 = write_clip(clip_paths[MP3_COVER], "mp3", AV_CODEC_ID_MP3, true) &&
		   write_clip(clip_paths[MP3_PLAIN], "mp3", AV_CODEC_ID_MP3, false);
	if (!have_mp3)
		fprintf(stderr, "no MP3 encoder in this FFmpeg; the MP3 cases are skipped\n");

	if (!write_clip(clip_paths[FLAC_COVER], "flac", AV_CODEC_ID_FLAC, true) ||
	    !write_clip(clip_paths[FLAC_PLAIN], "flac", AV_CODEC_ID_FLAC, false)) {
		fprintf(stderr, "could not write the FLAC test files\n");
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
		for (int c = 0; c < CLIP_COUNT; c++)
			os_unlink(clip_paths[c]);
		obs_up = false;
	}
	return 0;
}

/* ------------------------------------------------------------------------ */
/* Program audio */

struct heard {
	pthread_mutex_t mutex;
	bool armed;
	uint64_t frames;
};

static void raw_audio(void *param, size_t mix_idx, struct audio_data *data)
{
	UNUSED_PARAMETER(mix_idx);
	struct heard *h = param;

	const float *left = (const float *)data->data[0];
	bool tone = false;
	for (uint32_t i = 0; i < data->frames && !tone; i++)
		tone = left[i] > HEARD_LEVEL || left[i] < -HEARD_LEVEL;

	pthread_mutex_lock(&h->mutex);
	if (h->armed && tone)
		h->frames += data->frames;
	pthread_mutex_unlock(&h->mutex);
}

static void arm(struct heard *h)
{
	pthread_mutex_lock(&h->mutex);
	h->armed = true;
	h->frames = 0;
	pthread_mutex_unlock(&h->mutex);
}

static int64_t heard_ms(struct heard *h)
{
	pthread_mutex_lock(&h->mutex);
	int64_t ms = (int64_t)(h->frames * 1000 / RATE);
	pthread_mutex_unlock(&h->mutex);
	return ms;
}

static const char *state_name(enum obs_media_state s)
{
	switch (s) {
	case OBS_MEDIA_STATE_PLAYING:
		return "playing";
	case OBS_MEDIA_STATE_PAUSED:
		return "paused";
	case OBS_MEDIA_STATE_STOPPED:
		return "stopped";
	case OBS_MEDIA_STATE_ENDED:
		return "ended";
	case OBS_MEDIA_STATE_OPENING:
		return "opening";
	default:
		return "other";
	}
}

/* ------------------------------------------------------------------------ */

struct player {
	obs_source_t *media;
	obs_scene_t *scene;
	obs_scene_t *other;
	struct heard heard;
};

static void player_start(struct player *p, enum clip c)
{
	memset(p, 0, sizeof(*p));
	pthread_mutex_init(&p->heard.mutex, NULL);

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", clip_paths[c]);
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "restart_on_activate", true);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "hw_decode", false);
	p->media = obs_source_create("ffmpeg_source", "cover art media", settings, NULL);
	obs_data_release(settings);
	if (!p->media) {
		fprintf(stderr, "could not create the media source\n");
		fail();
	}

	p->scene = obs_scene_create("music");
	p->other = obs_scene_create("elsewhere");
	obs_scene_add(p->scene, p->media);

	obs_add_raw_audio_callback(0, NULL, raw_audio, &p->heard);
	obs_set_output_source(0, obs_scene_get_source(p->scene));
}

static void player_stop(struct player *p)
{
	obs_remove_raw_audio_callback(0, raw_audio, &p->heard);
	obs_set_output_source(0, NULL);
	/* the main canvas holds a reference to its scenes until they are removed */
	obs_source_remove(obs_scene_get_source(p->scene));
	obs_source_remove(obs_scene_get_source(p->other));
	obs_scene_release(p->scene);
	obs_scene_release(p->other);
	obs_source_release(p->media);
	pthread_mutex_destroy(&p->heard.mutex);
}

/* Waits until the source plays and the tone reaches the program. */
static void wait_playing(struct player *p, const char *what)
{
	arm(&p->heard);
	int64_t t = 0;
	enum obs_media_state s = OBS_MEDIA_STATE_NONE;
	for (int i = 0; i < 300; i++) {
		os_sleep_ms(10);
		s = obs_source_media_get_state(p->media);
		t = obs_source_media_get_time(p->media);
		if (s == OBS_MEDIA_STATE_PLAYING && t >= 300 && heard_ms(&p->heard) >= 200)
			return;
	}
	fprintf(stderr, "%s: playback did not start (%s, at %lld ms, tone heard for %lld ms)\n", what, state_name(s),
		(long long)t, (long long)heard_ms(&p->heard));
	fail();
}

struct watch {
	enum obs_media_state state;
	int64_t time_ms;
	int64_t min_time_ms;
	int64_t heard_ms;
};

/* Watches playback for WATCH_MS: the lowest media time from 200 ms on (once
 * a seek has settled), and the state, time and tone heard at the end. */
static struct watch watch_playback(struct player *p)
{
	struct watch w = {.min_time_ms = INT64_MAX};
	arm(&p->heard);
	for (int i = 0; i < WATCH_MS / 10; i++) {
		os_sleep_ms(10);
		int64_t t = obs_source_media_get_time(p->media);
		if (i >= 20 && t < w.min_time_ms)
			w.min_time_ms = t;
	}
	w.state = obs_source_media_get_state(p->media);
	w.time_ms = obs_source_media_get_time(p->media);
	w.heard_ms = heard_ms(&p->heard);
	return w;
}

static void seek_into_middle(enum clip c)
{
	if (c <= MP3_PLAIN && !have_mp3)
		skip();

	struct player p;
	player_start(&p, c);
	wait_playing(&p, clip_names[c]);

	int64_t before = obs_source_media_get_time(p.media);
	obs_source_media_set_time(p.media, SEEK_MS);
	struct watch w = watch_playback(&p);

	player_stop(&p);

	fprintf(stderr,
		"%s: seek from %lld ms to %d ms; %d ms later: %s at %lld ms (lowest %lld ms), tone heard for "
		"%lld ms\n",
		clip_names[c], (long long)before, SEEK_MS, WATCH_MS, state_name(w.state), (long long)w.time_ms,
		(long long)w.min_time_ms, (long long)w.heard_ms);

	if (w.min_time_ms < SEEK_MS - 100) {
		fprintf(stderr, "%s: playback went back to %lld ms after the seek to %d ms\n", clip_names[c],
			(long long)w.min_time_ms, SEEK_MS);
		fail();
	}
	if (w.state != OBS_MEDIA_STATE_PLAYING || w.time_ms < SEEK_MS + WATCH_MS / 2) {
		fprintf(stderr, "%s: playback did not continue from the seek position (%s at %lld ms)\n", clip_names[c],
			state_name(w.state), (long long)w.time_ms);
		fail();
	}
	if (w.heard_ms < MIN_HEARD_MS) {
		fprintf(stderr, "%s: the tone was heard for only %lld ms of the %d ms after the seek\n", clip_names[c],
			(long long)w.heard_ms, WATCH_MS);
		fail();
	}
}

/* The report: seeking an MP3 with an album cover. */
static void mp3_with_cover_seeks(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	seek_into_middle(MP3_COVER);
}

/* The same without the MP3 encoder: any attached picture does it. */
static void flac_with_cover_seeks(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	seek_into_middle(FLAC_COVER);
}

/* Guards: the same files without a cover seek on master. */
static void mp3_without_cover_seeks(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	seek_into_middle(MP3_PLAIN);
}

static void flac_without_cover_seeks(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	seek_into_middle(FLAC_PLAIN);
}

/* #9428/#11739: after the program was on another scene, showing the scene
 * again restarts playback, and the tone must come back from the start. Then
 * a seek after the restart must work as well. */
static void cover_restarts_and_seeks_after_reactivation(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();

	enum clip c = have_mp3 ? MP3_COVER : FLAC_COVER;
	struct player p;
	player_start(&p, c);
	wait_playing(&p, clip_names[c]);

	obs_set_output_source(0, obs_scene_get_source(p.other));
	os_sleep_ms(500);
	obs_set_output_source(0, obs_scene_get_source(p.scene));

	uint64_t shown = os_gettime_ns();
	wait_playing(&p, "after reactivation");
	int64_t restarted_at = obs_source_media_get_time(p.media);
	int64_t start_ms = (int64_t)((os_gettime_ns() - shown) / 1000000);

	obs_source_media_set_time(p.media, SEEK_MS);
	struct watch w = watch_playback(&p);

	player_stop(&p);

	fprintf(stderr,
		"%s, shown again: playing again after %lld ms at %lld ms; seek to %d ms, %d ms later: %s at %lld "
		"ms (lowest %lld ms), tone heard for %lld ms\n",
		clip_names[c], (long long)start_ms, (long long)restarted_at, SEEK_MS, WATCH_MS, state_name(w.state),
		(long long)w.time_ms, (long long)w.min_time_ms, (long long)w.heard_ms);

	if (restarted_at > 1500) {
		fprintf(stderr, "playback did not restart from the beginning when the scene was shown again\n");
		fail();
	}
	if (w.min_time_ms < SEEK_MS - 100 || w.state != OBS_MEDIA_STATE_PLAYING || w.time_ms < SEEK_MS + WATCH_MS / 2 ||
	    w.heard_ms < MIN_HEARD_MS) {
		fprintf(stderr, "the seek after the restart did not continue from the seek position\n");
		fail();
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(mp3_with_cover_seeks),
		cmocka_unit_test(flac_with_cover_seeks),
		cmocka_unit_test(mp3_without_cover_seeks),
		cmocka_unit_test(flac_without_cover_seeks),
		cmocka_unit_test(cover_restarts_and_seeks_after_reactivation),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
