/* Test-only FFmpeg stand-in for the media-remux oracle and the Rust port's
 * adapter (oracle/ffmpeg-shim/ffmpeg_shim.h). A test loads a script (the
 * input file's streams and packets and the result of every call), runs one
 * side, and reads back the log of every FFmpeg call with its arguments and
 * the fields the caller set. Both sides call this same code, so Tier 3
 * compares what they asked FFmpeg to do. The av_rescale_* functions follow
 * libavutil/mathematics.c. */
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffmpeg-shim/ffmpeg_shim.h"

#define REMUX_MAX_STREAMS 8
#define REMUX_MAX_PACKETS 64
#define REMUX_LOG_SIZE 65536

/* Mirrored by OracleRemuxStream in rust/obs-c-oracle/src/media_remux.rs. */
struct oracle_remux_stream {
	int32_t codec_type;
	int32_t codec_id;
	int32_t sample_rate;
	int32_t frame_size;
	int32_t channels;
	AVRational in_time_base;
	AVRational out_time_base;
	/* What av_get_audio_frame_duration2 returns for this stream. */
	int32_t frame_duration;
};

/* Mirrored by OracleRemuxPacket. */
struct oracle_remux_packet {
	int32_t stream_index;
	int32_t size;
	int64_t pts;
	int64_t dts;
	int64_t duration;
	int64_t pos;
	/* What av_interleaved_write_frame returns for it. */
	int32_t write_ret;
};

/* Mirrored by OracleRemuxScript. */
struct oracle_remux_script {
	int32_t open_ret;
	int32_t find_info_ret;
	bool alloc_ok;
	int32_t out_flags;
	bool has_codec_tag;
	/* What av_codec_get_id returns for any tag. */
	int32_t tag_codec_id;
	uint32_t nb_streams;
	/* avformat_new_stream returns NULL for this output stream (or never). */
	uint32_t new_stream_fail_at;
	/* avcodec_parameters_copy returns copy_ret for this stream. */
	uint32_t copy_fail_at;
	int32_t copy_ret;
	int32_t avio_ret;
	int32_t header_ret;
	int32_t trailer_ret;
	/* What av_read_frame returns after the last packet. */
	int32_t read_end_ret;
	uint32_t nb_packets;
	struct oracle_remux_stream streams[REMUX_MAX_STREAMS];
	struct oracle_remux_packet packets[REMUX_MAX_PACKETS];
};

#if defined(__GNUC__) || defined(__clang__)
#define REMUX_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define REMUX_PRINTF(fmt, args)
#endif

void oracle_remux_ffmpeg_reset(const struct oracle_remux_script *script);
size_t oracle_remux_ffmpeg_log(char *out, size_t size);
size_t oracle_remux_ffmpeg_live(void);

static struct oracle_remux_script script;
static char call_log[REMUX_LOG_SIZE];
static size_t call_log_len;
static uint32_t next_packet;
/* Contexts, streams and parameters handed out and not yet freed. */
static long live;

static AVOutputFormat output_format;
/* The contexts handed out, to name streams in the log by index. */
static AVFormatContext *input_ctx;
static AVFormatContext *output_ctx;

static int stream_of(const AVFormatContext *ctx, const AVCodecParameters *par)
{
	for (unsigned i = 0; ctx && i < ctx->nb_streams; i++)
		if (ctx->streams[i]->codecpar == par)
			return (int)i;
	return -1;
}
static const struct AVCodecTag *const fake_tags[1] = {NULL};

static void record(const char *format, ...) REMUX_PRINTF(1, 2);

static void record(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (call_log_len < REMUX_LOG_SIZE) {
		int n = vsnprintf(call_log + call_log_len, REMUX_LOG_SIZE - call_log_len, format, args);
		if (n > 0)
			call_log_len += (size_t)n;
		if (call_log_len >= REMUX_LOG_SIZE)
			call_log_len = REMUX_LOG_SIZE - 1;
		else
			call_log[call_log_len++] = '\n';
	}
	va_end(args);
}

static const char *str(const char *s)
{
	return s ? s : "(null)";
}

void oracle_remux_ffmpeg_reset(const struct oracle_remux_script *new_script)
{
	script = *new_script;
	if (script.nb_streams > REMUX_MAX_STREAMS)
		script.nb_streams = REMUX_MAX_STREAMS;
	if (script.nb_packets > REMUX_MAX_PACKETS)
		script.nb_packets = REMUX_MAX_PACKETS;
	call_log_len = 0;
	call_log[0] = 0;
	next_packet = 0;
	live = 0;
	input_ctx = NULL;
	output_ctx = NULL;
}

size_t oracle_remux_ffmpeg_log(char *out, size_t size)
{
	size_t n = call_log_len < size ? call_log_len : size;
	memcpy(out, call_log, n);
	return call_log_len;
}

size_t oracle_remux_ffmpeg_live(void)
{
	return (size_t)live;
}

static AVStream *new_stream(int index)
{
	AVStream *stream = calloc(1, sizeof(*stream));
	stream->index = index;
	stream->codecpar = calloc(1, sizeof(*stream->codecpar));
	live += 2;
	return stream;
}

static void free_context(AVFormatContext *ctx)
{
	if (ctx == input_ctx)
		input_ctx = NULL;
	if (ctx == output_ctx)
		output_ctx = NULL;
	for (unsigned i = 0; i < ctx->nb_streams; i++) {
		free(ctx->streams[i]->codecpar);
		free(ctx->streams[i]);
		live -= 2;
	}
	free(ctx->streams);
	free(ctx);
	live--;
}

int avformat_open_input(AVFormatContext **ps, const char *url, const AVInputFormat *fmt, AVDictionary **options)
{
	record("avformat_open_input %s fmt=%d options=%d", str(url), fmt != NULL, options != NULL);
	if (script.open_ret < 0) {
		*ps = NULL;
		return script.open_ret;
	}
	AVFormatContext *ctx = calloc(1, sizeof(*ctx));
	live++;
	ctx->nb_streams = script.nb_streams;
	ctx->streams = calloc(REMUX_MAX_STREAMS, sizeof(AVStream *));
	for (unsigned i = 0; i < script.nb_streams; i++) {
		const struct oracle_remux_stream *s = &script.streams[i];
		AVStream *stream = new_stream((int)i);
		stream->time_base = s->in_time_base;
		stream->codecpar->codec_type = (enum AVMediaType)s->codec_type;
		stream->codecpar->codec_id = (enum AVCodecID)s->codec_id;
		stream->codecpar->codec_tag = 0x11111111u * (i + 1);
		stream->codecpar->sample_rate = s->sample_rate;
		stream->codecpar->frame_size = s->frame_size;
		stream->codecpar->ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
		stream->codecpar->ch_layout.nb_channels = s->channels;
		stream->metadata = (AVDictionary *)(uintptr_t)(0x1000 + i);
		ctx->streams[i] = stream;
	}
	*ps = ctx;
	input_ctx = ctx;
	return script.open_ret;
}

int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options)
{
	record("avformat_find_stream_info in=%d options=%d", ic != NULL, options != NULL);
	return script.find_info_ret;
}

void av_dump_format(AVFormatContext *ic, int index, const char *url, int is_output)
{
	record("av_dump_format streams=%u index=%d %s output=%d", ic ? ic->nb_streams : 0u, index, str(url), is_output);
}

int avformat_alloc_output_context2(AVFormatContext **ctx, const AVOutputFormat *oformat, const char *format_name,
				   const char *filename)
{
	record("avformat_alloc_output_context2 oformat=%d format=%s %s", oformat != NULL, str(format_name),
	       str(filename));
	if (!script.alloc_ok) {
		*ctx = NULL;
		return AVERROR(22);
	}
	output_format.flags = script.out_flags;
	output_format.codec_tag = script.has_codec_tag ? fake_tags : NULL;
	AVFormatContext *out = calloc(1, sizeof(*out));
	live++;
	out->oformat = &output_format;
	out->streams = calloc(REMUX_MAX_STREAMS, sizeof(AVStream *));
	*ctx = out;
	output_ctx = out;
	return 0;
}

AVStream *avformat_new_stream(AVFormatContext *s, const void *c)
{
	record("avformat_new_stream index=%u codec=%d", s->nb_streams, c != NULL);
	if (s->nb_streams == script.new_stream_fail_at || s->nb_streams >= REMUX_MAX_STREAMS)
		return NULL;
	AVStream *stream = new_stream((int)s->nb_streams);
	s->streams[s->nb_streams++] = stream;
	return stream;
}

int avcodec_parameters_copy(AVCodecParameters *dst, const AVCodecParameters *src)
{
	int out = stream_of(output_ctx, dst);
	record("avcodec_parameters_copy out=%d in=%d", out, stream_of(input_ctx, src));
	if (out >= 0 && (uint32_t)out == script.copy_fail_at)
		return script.copy_ret;
	*dst = *src;
	return 0;
}

int av_dict_copy(AVDictionary **dst, const AVDictionary *src, int flags)
{
	record("av_dict_copy dst_empty=%d src=%" PRIuPTR " flags=%d", *dst == NULL, (uintptr_t)src, flags);
	*dst = (AVDictionary *)(uintptr_t)src;
	return 0;
}

enum AVCodecID av_codec_get_id(const struct AVCodecTag *const *tags, unsigned int tag)
{
	record("av_codec_get_id tags=%d tag=%08x", tags != NULL, tag);
	return (enum AVCodecID)script.tag_codec_id;
}

void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels)
{
	record("av_channel_layout_default %d", nb_channels);
	ch_layout->order = AV_CHANNEL_ORDER_NATIVE;
	ch_layout->nb_channels = nb_channels;
	ch_layout->u.mask = (UINT64_C(1) << (nb_channels & 63)) - 1;
	ch_layout->opaque = NULL;
}

int avio_open(AVIOContext **s, const char *url, int flags)
{
	record("avio_open %s flags=%d", str(url), flags);
	if (script.avio_ret >= 0) {
		*s = (AVIOContext *)calloc(1, 1);
		live++;
	}
	return script.avio_ret;
}

int avio_close(AVIOContext *s)
{
	record("avio_close %d", s != NULL);
	if (s) {
		free(s);
		live--;
	}
	return 0;
}

static void record_streams(AVFormatContext *s)
{
	for (unsigned i = 0; i < s->nb_streams; i++) {
		const AVCodecParameters *par = s->streams[i]->codecpar;
		record("  stream %u type=%d codec=%d tag=%08x rate=%d frame=%d layout=%d/%d/%" PRIx64
		       " metadata=%" PRIuPTR,
		       i, (int)par->codec_type, (int)par->codec_id, (unsigned)par->codec_tag, par->sample_rate,
		       par->frame_size, (int)par->ch_layout.order, par->ch_layout.nb_channels, par->ch_layout.u.mask,
		       (uintptr_t)s->streams[i]->metadata);
	}
}

int avformat_write_header(AVFormatContext *s, AVDictionary **options)
{
	record("avformat_write_header pb=%d options=%d", s->pb != NULL, options != NULL);
	record_streams(s);
	for (unsigned i = 0; i < s->nb_streams && i < script.nb_streams; i++)
		s->streams[i]->time_base = script.streams[i].out_time_base;
	return script.header_ret;
}

int av_read_frame(AVFormatContext *s, AVPacket *pkt)
{
	(void)s;
	if (next_packet >= script.nb_packets) {
		record("av_read_frame -> %d", script.read_end_ret);
		return script.read_end_ret;
	}
	const struct oracle_remux_packet *p = &script.packets[next_packet++];
	memset(pkt, 0, sizeof(*pkt));
	pkt->stream_index = p->stream_index;
	pkt->size = p->size;
	pkt->pts = p->pts;
	pkt->dts = p->dts;
	pkt->duration = p->duration;
	pkt->pos = p->pos;
	record("av_read_frame -> packet %u", next_packet - 1);
	return 0;
}

int av_interleaved_write_frame(AVFormatContext *s, AVPacket *pkt)
{
	(void)s;
	record("av_interleaved_write_frame stream=%d size=%d pts=%" PRId64 " dts=%" PRId64 " duration=%" PRId64
	       " pos=%" PRId64,
	       pkt->stream_index, pkt->size, pkt->pts, pkt->dts, pkt->duration, pkt->pos);
	return next_packet ? script.packets[next_packet - 1].write_ret : 0;
}

void av_packet_unref(AVPacket *pkt)
{
	record("av_packet_unref");
	memset(pkt, 0, sizeof(*pkt));
	pkt->pts = AV_NOPTS_VALUE;
	pkt->dts = AV_NOPTS_VALUE;
	pkt->pos = -1;
}

int av_write_trailer(AVFormatContext *s)
{
	(void)s;
	record("av_write_trailer");
	return script.trailer_ret;
}

void avformat_close_input(AVFormatContext **s)
{
	record("avformat_close_input %d", *s != NULL);
	if (*s)
		free_context(*s);
	*s = NULL;
}

void avformat_free_context(AVFormatContext *s)
{
	record("avformat_free_context %d", s != NULL);
	if (s)
		free_context(s);
}

int av_get_audio_frame_duration2(AVCodecParameters *par, int frame_bytes)
{
	int index = stream_of(input_ctx, par);
	record("av_get_audio_frame_duration2 stream=%d bytes=%d", index, frame_bytes);
	return index >= 0 ? script.streams[index].frame_duration : 0;
}

int av_strerror(int errnum, char *errbuf, size_t errbuf_size)
{
	snprintf(errbuf, errbuf_size, "error %d", errnum);
	return 0;
}

/* libavutil/mathematics.c */
int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding rnd)
{
	int64_t r = 0;
	int mode = (int)rnd & ~(int)AV_ROUND_PASS_MINMAX;

	if (c <= 0 || b < 0 || mode < 0 || mode > 5 || mode == 4)
		return INT64_MIN;

	if (rnd & AV_ROUND_PASS_MINMAX) {
		if (a == INT64_MIN || a == INT64_MAX)
			return a;
		rnd = (enum AVRounding)(rnd - AV_ROUND_PASS_MINMAX);
	}

	if (a < 0)
		return (int64_t)(0 - (uint64_t)av_rescale_rnd(-(a > -INT64_MAX ? a : -INT64_MAX), b, c,
							      (enum AVRounding)(rnd ^ ((rnd >> 1) & 1))));

	if (rnd == AV_ROUND_NEAR_INF)
		r = c / 2;
	else if (rnd & 1)
		r = c - 1;

	if (b <= INT_MAX && c <= INT_MAX) {
		if (a <= INT_MAX)
			return (a * b + r) / c;
		else {
			int64_t ad = a / c;
			int64_t a2 = (a % c * b + r) / c;
			if (ad >= INT32_MAX && b && ad > (INT64_MAX - a2) / b)
				return INT64_MIN;
			return ad * b + a2;
		}
	} else {
		uint64_t a0 = (uint64_t)a & 0xFFFFFFFF;
		uint64_t a1 = (uint64_t)a >> 32;
		uint64_t b0 = (uint64_t)b & 0xFFFFFFFF;
		uint64_t b1 = (uint64_t)b >> 32;
		uint64_t t1 = a0 * b1 + a1 * b0;
		uint64_t t1a = t1 << 32;
		int i;

		a0 = a0 * b0 + t1a;
		a1 = a1 * b1 + (t1 >> 32) + (a0 < t1a);
		a0 += (uint64_t)r;
		a1 += a0 < (uint64_t)r;

		for (i = 63; i >= 0; i--) {
			a1 += a1 + ((a0 >> i) & 1);
			t1 += t1;
			if ((uint64_t)c <= a1) {
				a1 -= (uint64_t)c;
				t1++;
			}
		}
		if (t1 > (uint64_t)INT64_MAX)
			return INT64_MIN;
		return (int64_t)t1;
	}
}

int64_t av_rescale_q_rnd(int64_t a, AVRational bq, AVRational cq, enum AVRounding rnd)
{
	int64_t b = bq.num * (int64_t)cq.den;
	int64_t c = cq.num * (int64_t)bq.den;
	return av_rescale_rnd(a, b, c, rnd);
}

int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq)
{
	return av_rescale_q_rnd(a, bq, cq, AV_ROUND_NEAR_INF);
}

int64_t av_rescale_delta(AVRational in_tb, int64_t in_ts, AVRational fs_tb, int duration, int64_t *last,
			 AVRational out_tb)
{
	int64_t a, b, this;

	record("av_rescale_delta in=%d/%d ts=%" PRId64 " fs=%d/%d duration=%d last=%" PRId64 " out=%d/%d", in_tb.num,
	       in_tb.den, in_ts, fs_tb.num, fs_tb.den, duration, *last, out_tb.num, out_tb.den);

	if (*last == AV_NOPTS_VALUE || !duration ||
	    in_tb.num * (int64_t)out_tb.den <= out_tb.num * (int64_t)in_tb.den) {
	simple_round:
		*last = av_rescale_q(in_ts, in_tb, fs_tb) + duration;
		return av_rescale_q(in_ts, in_tb, out_tb);
	}

	a = av_rescale_q_rnd(2 * in_ts - 1, in_tb, fs_tb, AV_ROUND_DOWN) >> 1;
	b = (av_rescale_q_rnd(2 * in_ts + 1, in_tb, fs_tb, AV_ROUND_UP) + 1) >> 1;
	if (*last < 2 * a - b || *last > 2 * b - a)
		goto simple_round;

	this = *last < a ? a : (*last > b ? b : *last);
	*last = this + duration;

	return av_rescale_q(this, fs_tb, out_tb);
}

/* The oracle and the adapter are compiled with blog renamed to this, so
 * their log lines land in the call log in order. */
void oracle_remux_blog(int level, const char *format, ...) REMUX_PRINTF(2, 3);

void oracle_remux_blog(int level, const char *format, ...)
{
	char line[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	record("blog %d %s", level, line);
}
