/*
 * Characterization of media_remux_job_* (libobs/media-io/media-remux.c.h,
 * Tools > Remux Recordings) before its port to obs-media-io: which jobs it
 * refuses to create, what it reports while it runs, and what it writes.
 * test_remux_audio_timestamps covers the audio timestamps.
 *
 * The inputs are WAV files written here byte by byte, so no encoder is
 * needed; FFmpeg's WAV demuxer and Matroska muxer read and write them.
 * No display is needed.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>

#include <util/bmem.h>
#include <util/platform.h>
#include <media-io/media-remux.h>

#define RATE 48000
/* Samples per channel: enough packets for several progress reports. */
#define SAMPLES (RATE * 4)

static char tmp_dir[400];
static uint64_t run_id;
static int file_seq;

static void make_path(char *dst, size_t size, const char *ext)
{
	snprintf(dst, size, "%s/obs-media-remux-%llu-%d.%s", tmp_dir, (unsigned long long)run_id, file_seq++, ext);
}

static void put_u16(FILE *f, uint16_t v)
{
	fputc(v & 0xff, f);
	fputc(v >> 8, f);
}

static void put_u32(FILE *f, uint32_t v)
{
	put_u16(f, (uint16_t)(v & 0xffff));
	put_u16(f, (uint16_t)(v >> 16));
}

/* A 16-bit PCM WAV of `channels` channels and SAMPLES samples of a ramp. */
static void write_wav(const char *path, int channels)
{
	FILE *f = fopen(path, "wb");
	assert_non_null(f);

	uint32_t data_size = (uint32_t)(SAMPLES * channels * 2);
	fwrite("RIFF", 1, 4, f);
	put_u32(f, 36 + data_size);
	fwrite("WAVEfmt ", 1, 8, f);
	put_u32(f, 16);
	put_u16(f, 1);
	put_u16(f, (uint16_t)channels);
	put_u32(f, RATE);
	put_u32(f, (uint32_t)(RATE * channels * 2));
	put_u16(f, (uint16_t)(channels * 2));
	put_u16(f, 16);
	fwrite("data", 1, 4, f);
	put_u32(f, data_size);
	for (int s = 0; s < SAMPLES; s++)
		for (int c = 0; c < channels; c++)
			put_u16(f, (uint16_t)(s * 7 + c * 1000));
	fclose(f);
}

static void write_text(const char *path)
{
	FILE *f = fopen(path, "wb");
	assert_non_null(f);
	fputs("this is not a media file\n", f);
	fclose(f);
}

struct audio_info {
	int channels;
	uint64_t mask;
	int sample_rate;
	int64_t samples;
};

/* The first audio stream of `path`: its layout and how many samples its
 * packets hold. */
static bool read_audio(const char *path, struct audio_info *info)
{
	AVFormatContext *ic = NULL;
	if (avformat_open_input(&ic, path, NULL, NULL) < 0)
		return false;

	bool ok = false;
	int idx = avformat_find_stream_info(ic, NULL) >= 0
			  ? av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0)
			  : -1;
	AVPacket *pkt = av_packet_alloc();
	if (idx < 0 || !pkt)
		goto done;

	const AVCodecParameters *par = ic->streams[idx]->codecpar;
	info->channels = par->ch_layout.nb_channels;
	info->mask = par->ch_layout.order == AV_CHANNEL_ORDER_NATIVE ? par->ch_layout.u.mask : 0;
	info->sample_rate = par->sample_rate;
	info->samples = 0;
	int frame_bytes = par->ch_layout.nb_channels * 2;

	while (av_read_frame(ic, pkt) >= 0) {
		if (pkt->stream_index == idx)
			info->samples += pkt->size / frame_bytes;
		av_packet_unref(pkt);
	}
	ok = true;

done:
	av_packet_free(&pkt);
	avformat_close_input(&ic);
	return ok;
}

/* ------------------------------------------------------------------------ */
/* Refused jobs */

static void create_without_job_pointer_fails(void **state)
{
	(void)state;
	char in[512], out[512];
	make_path(in, sizeof(in), "wav");
	make_path(out, sizeof(out), "mkv");
	write_wav(in, 2);

	assert_false(media_remux_job_create(NULL, in, out));
	assert_false(os_file_exists(out));
	os_unlink(in);
}

static void create_missing_input_fails(void **state)
{
	(void)state;
	char in[512], out[512];
	make_path(in, sizeof(in), "wav");
	make_path(out, sizeof(out), "mkv");

	media_remux_job_t job = (media_remux_job_t)(uintptr_t)1;
	assert_false(media_remux_job_create(&job, in, out));
	assert_null(job);

	job = (media_remux_job_t)(uintptr_t)1;
	assert_false(media_remux_job_create(&job, NULL, out));
	assert_null(job);
	assert_false(os_file_exists(out));
}

static void create_same_file_fails(void **state)
{
	(void)state;
	char in[512];
	make_path(in, sizeof(in), "wav");
	write_wav(in, 2);

	media_remux_job_t job = (media_remux_job_t)(uintptr_t)1;
	assert_false(media_remux_job_create(&job, in, in));
	assert_null(job);
	assert_int_equal(os_get_file_size(in), 44 + SAMPLES * 2 * 2);
	os_unlink(in);
}

static void create_unreadable_input_fails(void **state)
{
	(void)state;
	char in[512], out[512];
	make_path(in, sizeof(in), "txt");
	make_path(out, sizeof(out), "mkv");
	write_text(in);

	long before = bnum_allocs();
	media_remux_job_t job = NULL;
	assert_false(media_remux_job_create(&job, in, out));
	/* job: see dangling_job_after_failure. */
	(void)job;
	assert_int_equal(bnum_allocs(), before);
	assert_false(os_file_exists(out));
	os_unlink(in);
}

static void create_unknown_output_format_fails(void **state)
{
	(void)state;
	char in[512], out[512];
	make_path(in, sizeof(in), "wav");
	make_path(out, sizeof(out), "not-a-container");
	write_wav(in, 2);

	long before = bnum_allocs();
	media_remux_job_t job = NULL;
	assert_false(media_remux_job_create(&job, in, out));
	/* job: see dangling_job_after_failure. */
	(void)job;
	assert_int_equal(bnum_allocs(), before);
	assert_false(os_file_exists(out));
	os_unlink(in);
}

static void create_output_in_missing_directory_fails(void **state)
{
	(void)state;
	char in[512], out[600];
	make_path(in, sizeof(in), "wav");
	snprintf(out, sizeof(out), "%s/obs-media-remux-%llu-missing/out.mkv", tmp_dir, (unsigned long long)run_id);
	write_wav(in, 2);

	long before = bnum_allocs();
	media_remux_job_t job = NULL;
	assert_false(media_remux_job_create(&job, in, out));
	/* job: see dangling_job_after_failure. */
	(void)job;
	assert_int_equal(bnum_allocs(), before);
	os_unlink(in);
}

/* Characterized, not endorsed: once the job exists (the input exists and
 * the names differ), a failed create destroys it but leaves *job pointing
 * at the freed job. Callers must not touch it; the frontend destroys only a
 * created job. Nothing here reads it. */

static void null_job_is_ignored(void **state)
{
	(void)state;
	assert_false(media_remux_job_process(NULL, NULL, NULL));
	media_remux_job_destroy(NULL);
}

/* ------------------------------------------------------------------------ */
/* Remuxing */

#define MAX_REPORTS 1024

struct progress {
	int calls;
	float percent[MAX_REPORTS];
	/* Return false from this call on (0 for never). */
	int stop_at;
};

static bool on_progress(void *data, float percent)
{
	struct progress *p = data;
	if (p->calls < MAX_REPORTS)
		p->percent[p->calls] = percent;
	p->calls++;
	return !(p->stop_at && p->calls >= p->stop_at);
}

static void remux(int channels, const char *ext, struct progress *progress, struct audio_info *out_info,
		  bool *processed)
{
	char in[512], out[512];
	make_path(in, sizeof(in), "wav");
	make_path(out, sizeof(out), ext);
	write_wav(in, channels);

	media_remux_job_t job = NULL;
	assert_true(media_remux_job_create(&job, in, out));
	assert_non_null(job);
	*processed = media_remux_job_process(job, progress ? on_progress : NULL, progress);
	media_remux_job_destroy(job);

	assert_true(read_audio(out, out_info));
	os_unlink(in);
	os_unlink(out);
}

static void remux_keeps_every_sample(void **state)
{
	(void)state;
	struct audio_info info;
	bool processed;
	remux(2, "wav", NULL, &info, &processed);

	assert_true(processed);
	assert_int_equal(info.channels, 2);
	assert_int_equal(info.sample_rate, RATE);
	assert_int_equal(info.samples, SAMPLES);
}

/* The default layout for 5 channels is 5.0; OBS writes 4.1. WAV keeps the
 * layout (WAVE_FORMAT_EXTENSIBLE), Matroska only the channel count. */
static void five_channels_become_4point1(void **state)
{
	(void)state;
	struct audio_info info;
	bool processed;
	remux(5, "wav", NULL, &info, &processed);

	assert_true(processed);
	assert_int_equal(info.channels, 5);
	assert_int_equal(info.mask, AV_CH_LAYOUT_4POINT1);
	assert_int_equal(info.samples, SAMPLES);
}

static void six_channels_get_the_default_layout(void **state)
{
	(void)state;
	struct audio_info info;
	bool processed;
	remux(6, "wav", NULL, &info, &processed);

	assert_true(processed);
	assert_int_equal(info.channels, 6);
	assert_int_equal(info.mask, AV_CH_LAYOUT_5POINT1_BACK);
}

/* 0 first, 100 last, and in between the read position over the input's
 * size, rising, once every 12 packets. */
static void progress_runs_from_0_to_100(void **state)
{
	(void)state;
	static struct progress p;
	memset(&p, 0, sizeof(p));
	struct audio_info info;
	bool processed;
	remux(2, "mkv", &p, &info, &processed);

	assert_true(processed);
	assert_true(p.calls >= 4);
	assert_true(p.calls <= MAX_REPORTS);
	assert_true(p.percent[0] == 0.f);
	assert_true(p.percent[p.calls - 1] == 100.f);
	for (int i = 1; i < p.calls - 1; i++) {
		assert_true(p.percent[i] > 0.f);
		assert_true(p.percent[i] < 100.f);
		if (i > 1)
			assert_true(p.percent[i] > p.percent[i - 1]);
	}
	assert_int_equal(info.samples, SAMPLES);
}

/* Characterized, not endorsed: a callback that returns false stops the
 * remux, yet the job reports success and the output keeps what was written
 * so far; the callback still gets its 100. */
static void stopping_from_the_callback_still_succeeds(void **state)
{
	(void)state;
	static struct progress p;
	memset(&p, 0, sizeof(p));
	p.stop_at = 2;
	struct audio_info info;
	bool processed;
	remux(2, "mkv", &p, &info, &processed);

	assert_true(processed);
	assert_int_equal(p.calls, 3);
	assert_true(p.percent[0] == 0.f);
	assert_true(p.percent[1] > 0.f);
	assert_true(p.percent[2] == 100.f);
	assert_true(info.samples > 0);
	assert_true(info.samples < SAMPLES);
}

/* IVF holds only VP8, VP9 and AV1, so its muxer rejects PCM audio:
 * create succeeds and process fails at the header. */
static void process_fails_when_the_muxer_rejects_the_stream(void **state)
{
	(void)state;
	char in[512], out[512];
	make_path(in, sizeof(in), "wav");
	make_path(out, sizeof(out), "ivf");
	write_wav(in, 2);

	media_remux_job_t job = NULL;
	bool created = media_remux_job_create(&job, in, out);
	if (created) {
		static struct progress p;
		memset(&p, 0, sizeof(p));
		assert_false(media_remux_job_process(job, on_progress, &p));
		/* No progress report when the header fails. */
		assert_int_equal(p.calls, 0);
	} else {
		assert_null(job);
	}
	media_remux_job_destroy(job);
	os_unlink(in);
	os_unlink(out);
}

int main(void)
{
	av_log_set_level(AV_LOG_QUIET);

	const char *tmp = getenv("TMPDIR");
#ifdef _WIN32
	if (!tmp)
		tmp = getenv("TEMP");
#endif
	snprintf(tmp_dir, sizeof(tmp_dir), "%s", tmp && *tmp ? tmp : "/tmp");
	run_id = os_gettime_ns();

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(create_without_job_pointer_fails),
		cmocka_unit_test(create_missing_input_fails),
		cmocka_unit_test(create_same_file_fails),
		cmocka_unit_test(create_unreadable_input_fails),
		cmocka_unit_test(create_unknown_output_format_fails),
		cmocka_unit_test(create_output_in_missing_directory_fails),
		cmocka_unit_test(null_job_is_ignored),
		cmocka_unit_test(remux_keeps_every_sample),
		cmocka_unit_test(five_channels_become_4point1),
		cmocka_unit_test(six_channels_get_the_default_layout),
		cmocka_unit_test(progress_runs_from_0_to_100),
		cmocka_unit_test(stopping_from_the_callback_still_succeeds),
		cmocka_unit_test(process_fails_when_the_muxer_rejects_the_stream),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
