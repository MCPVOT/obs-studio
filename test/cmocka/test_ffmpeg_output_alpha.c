/* A Custom FFmpeg Output recording WebM/VP9 with the BGRA (alpha) colour
 * format failed to start with "Failed to open video codec: Invalid
 * argument" (obsproject/obs-studio#13785, reported by PickKali). The output
 * picks the encoder pixel format closest to BGRA from the formats the
 * encoder lists. FFmpeg 8 added gbrap to libvpx-vp9's list, so gbrap wins,
 * but libvpx-vp9 refuses gbrap unless strict experimental compliance is on.
 * The supported way to record VP9 with alpha is yuva420p.
 *
 * The test builds the real ffmpeg_output (obs-ffmpeg-output.c) into the
 * test, records a short clip from a BGRA, sRGB, full range core video
 * mix to a temporary file, then reads the file back with libavformat. The
 * guards check that encoders which accept the first choice keep it: FFV1 in
 * Matroska still records bgra, and a VP8 alpha recording that libvpx
 * rejects for another reason (auto-alt-ref) still fails rather than being
 * recorded silently without alpha. obs_startup() and core video need a
 * display, so the cases skip without one (run under xvfb-run). */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include <obs.h>
#include <obs-module.h>
#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>

#define W 128
#define H 128

extern struct obs_output_info ffmpeg_output;

static bool obs_up;
static char clip_path[512];

const char *obs_module_text(const char *val)
{
	return val;
}

static void quiet_log(int lvl, const char *msg, va_list args, void *p)
{
	(void)p;
	if (lvl <= LOG_WARNING) {
		vfprintf(stderr, msg, args);
		fputc('\n', stderr);
	}
}

struct output_events {
	os_event_t *started;
	os_event_t *stopped;
	long long stop_code;
};

static void on_start(void *param, calldata_t *cd)
{
	(void)cd;
	struct output_events *ev = param;
	os_event_signal(ev->started);
}

static void on_stop(void *param, calldata_t *cd)
{
	struct output_events *ev = param;
	ev->stop_code = calldata_int(cd, "code");
	os_event_signal(ev->stopped);
}

static bool have_encoders(const char *video, const char *audio)
{
	if (!avcodec_find_encoder_by_name(video)) {
		fprintf(stderr, "skipped: FFmpeg has no %s encoder\n", video);
		return false;
	}
	if (!avcodec_find_encoder_by_name(audio)) {
		fprintf(stderr, "skipped: FFmpeg has no %s encoder\n", audio);
		return false;
	}
	return true;
}

/* Records about one second through ffmpeg_output. Returns whether the
 * output started; `error` receives the output's last error. */
static bool record_clip(const char *format_name, const char *ext, const char *video_encoder, const char *audio_encoder,
			const char *video_settings, char *error, size_t error_size)
{
	const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
	snprintf(clip_path, sizeof(clip_path), "%s/obs-ffmpeg-output-alpha-%llu.%s", tmp,
		 (unsigned long long)os_gettime_ns(), ext);
	error[0] = 0;

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "url", clip_path);
	obs_data_set_string(settings, "format_name", format_name);
	obs_data_set_string(settings, "video_encoder", video_encoder);
	obs_data_set_string(settings, "audio_encoder", audio_encoder);
	obs_data_set_string(settings, "video_settings", video_settings);
	obs_data_set_int(settings, "video_bitrate", 1000);
	obs_data_set_int(settings, "audio_bitrate", 128);
	obs_output_t *output = obs_output_create("ffmpeg_output", "alpha test", settings, NULL);
	obs_data_release(settings);
	assert_non_null(output);
	obs_output_set_mixers(output, 1);

	struct output_events ev = {0};
	os_event_init(&ev.started, OS_EVENT_TYPE_MANUAL);
	os_event_init(&ev.stopped, OS_EVENT_TYPE_MANUAL);
	signal_handler_t *sh = obs_output_get_signal_handler(output);
	signal_handler_connect(sh, "start", on_start, &ev);
	signal_handler_connect(sh, "stop", on_stop, &ev);

	bool started = false;
	if (obs_output_start(output)) {
		/* ffmpeg_output connects on a thread: it either begins data
		 * capture ("start") or signals a failed connect ("stop"). */
		for (int i = 0; i < 1000; i++) {
			if (os_event_try(ev.started) == 0) {
				started = true;
				break;
			}
			if (os_event_try(ev.stopped) == 0)
				break;
			os_sleep_ms(10);
		}
	}

	if (started) {
		os_sleep_ms(1000);
		obs_output_stop(output);
		if (os_event_timedwait(ev.stopped, 10000) != 0) {
			fprintf(stderr, "the output did not stop\n");
			fail();
		}
	} else {
		const char *last = obs_output_get_last_error(output);
		snprintf(error, error_size, "%s (stop code %lld)", last ? last : "no error", ev.stop_code);
	}

	signal_handler_disconnect(sh, "start", on_start, &ev);
	signal_handler_disconnect(sh, "stop", on_stop, &ev);
	obs_output_release(output);
	os_event_destroy(ev.started);
	os_event_destroy(ev.stopped);
	return started;
}

struct clip_info {
	enum AVCodecID codec_id;
	int format;
	bool alpha_mode;
	int video_packets;
};

static bool probe_clip(struct clip_info *info)
{
	AVFormatContext *fmt = NULL;
	if (avformat_open_input(&fmt, clip_path, NULL, NULL) < 0)
		return false;
	if (avformat_find_stream_info(fmt, NULL) < 0) {
		avformat_close_input(&fmt);
		return false;
	}

	int vidx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (vidx < 0) {
		avformat_close_input(&fmt);
		return false;
	}

	AVStream *st = fmt->streams[vidx];
	AVDictionaryEntry *alpha = av_dict_get(st->metadata, "alpha_mode", NULL, 0);
	info->codec_id = st->codecpar->codec_id;
	info->format = st->codecpar->format;
	info->alpha_mode = alpha && strcmp(alpha->value, "1") == 0;
	info->video_packets = 0;

	AVPacket *pkt = av_packet_alloc();
	while (av_read_frame(fmt, pkt) >= 0) {
		if (pkt->stream_index == vidx)
			info->video_packets++;
		av_packet_unref(pkt);
	}
	av_packet_free(&pkt);
	avformat_close_input(&fmt);
	return true;
}

static void vp9_webm_bgra_records_with_alpha(void **state)
{
	(void)state;
	if (!obs_up)
		skip();
	if (!have_encoders("libvpx-vp9", "libopus"))
		skip();

	char error[512];
	bool started = record_clip("webm", "webm", "libvpx-vp9", "libopus", "", error, sizeof(error));
	if (!started) {
		os_unlink(clip_path);
		fprintf(stderr, "WebM/VP9 with BGRA did not start: %s\n", error);
		fail();
	}

	struct clip_info info;
	bool probed = probe_clip(&info);
	os_unlink(clip_path);
	if (!probed) {
		fprintf(stderr, "could not read the recorded WebM back\n");
		fail();
	}
	if (info.codec_id != AV_CODEC_ID_VP9 || info.video_packets < 1) {
		fprintf(stderr, "expected VP9 video packets, got codec %s with %d packets\n",
			avcodec_get_name(info.codec_id), info.video_packets);
		fail();
	}
	if (!info.alpha_mode) {
		fprintf(stderr, "the WebM video stream has no alpha (alpha_mode is not 1)\n");
		fail();
	}
}

static void ffv1_mkv_bgra_keeps_bgra(void **state)
{
	(void)state;
	if (!obs_up)
		skip();
	if (!have_encoders("ffv1", "libopus"))
		skip();

	char error[512];
	bool started = record_clip("matroska", "mkv", "ffv1", "libopus", "", error, sizeof(error));
	if (!started) {
		os_unlink(clip_path);
		fprintf(stderr, "Matroska/FFV1 with BGRA did not start: %s\n", error);
		fail();
	}

	struct clip_info info;
	bool probed = probe_clip(&info);
	os_unlink(clip_path);
	if (!probed) {
		fprintf(stderr, "could not read the recorded Matroska file back\n");
		fail();
	}
	if (info.codec_id != AV_CODEC_ID_FFV1 || info.video_packets < 1) {
		fprintf(stderr, "expected FFV1 video packets, got codec %s with %d packets\n",
			avcodec_get_name(info.codec_id), info.video_packets);
		fail();
	}
	if (info.format != AV_PIX_FMT_BGRA) {
		const char *name = av_get_pix_fmt_name(info.format);
		fprintf(stderr, "FFV1 recorded %s instead of bgra\n", name ? name : "an unknown format");
		fail();
	}
}

static void vp8_alpha_conflict_still_fails(void **state)
{
	(void)state;
	if (!obs_up)
		skip();
	if (!have_encoders("libvpx", "libopus"))
		skip();

	/* libvpx (VP8) accepts yuva420p, the first choice for BGRA, but not
	 * together with its default auto-alt-ref. Recording without alpha
	 * would hide that; the user fixes it with auto-alt-ref=0. */
	char error[512];
	bool started = record_clip("webm", "webm", "libvpx", "libopus", "", error, sizeof(error));
	os_unlink(clip_path);
	if (started) {
		fprintf(stderr, "VP8 with alpha and auto-alt-ref started; alpha was dropped\n");
		fail();
	}

	started = record_clip("webm", "webm", "libvpx", "libopus", "auto-alt-ref=0", error, sizeof(error));
	if (!started) {
		os_unlink(clip_path);
		fprintf(stderr, "VP8 with alpha and auto-alt-ref=0 did not start: %s\n", error);
		fail();
	}

	struct clip_info info;
	bool probed = probe_clip(&info);
	os_unlink(clip_path);
	if (!probed || info.codec_id != AV_CODEC_ID_VP8 || !info.alpha_mode) {
		fprintf(stderr, "expected a VP8 WebM with alpha\n");
		fail();
	}
}

static int setup(void **state)
{
	(void)state;
	base_set_log_handler(quiet_log, NULL);

	const char *display = getenv("DISPLAY");
	if (!display || !*display) {
		fprintf(stderr, "skipped: no display\n");
		return 0;
	}

	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "skipped: obs_startup failed\n");
		return 0;
	}

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = W,
		.base_height = H,
		.output_width = W,
		.output_height = H,
		.output_format = VIDEO_FORMAT_BGRA,
		.colorspace = VIDEO_CS_SRGB,
		.range = VIDEO_RANGE_FULL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS || !obs_reset_audio(&oai)) {
		fprintf(stderr, "skipped: core video/audio failed\n");
		obs_shutdown();
		return 0;
	}

	obs_register_output(&ffmpeg_output);
	obs_up = true;
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
		cmocka_unit_test(vp9_webm_bgra_records_with_alpha),
		cmocka_unit_test(ffv1_mkv_bgra_keeps_bgra),
		cmocka_unit_test(vp8_alpha_conflict_still_fails),
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
