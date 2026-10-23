/* Test-only stand-in for the parts of FFmpeg that libobs/media-io/media-remux.c.h
 * and its adapter use (oracle/ffmpeg-shim). The Rust test runners have no
 * FFmpeg (Windows has not even zlib), so the original C and the Rust port's
 * adapter are both compiled against these declarations and call the same
 * scripted, recording implementation (oracle/media_remux_ffmpeg.c). Only
 * the fields and values the remuxer touches exist; their names and types
 * follow FFmpeg 7/8. */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
#define MKTAG(a, b, c, d) ((unsigned)(a) | ((unsigned)(b) << 8) | ((unsigned)(c) << 16) | ((unsigned)(d) << 24))
#define FFERRTAG(a, b, c, d) (-(int)MKTAG(a, b, c, d))
#define AVERROR(e) (-(e))
#define AVERROR_EOF FFERRTAG('E', 'O', 'F', ' ')
#define AVERROR_INVALIDDATA FFERRTAG('I', 'N', 'D', 'A')
#define AV_ERROR_MAX_STRING_SIZE 64
#define AVFMT_NOFILE 0x0001
#define AVIO_FLAG_WRITE 2

enum AVMediaType {
	AVMEDIA_TYPE_UNKNOWN = -1,
	AVMEDIA_TYPE_VIDEO,
	AVMEDIA_TYPE_AUDIO,
	AVMEDIA_TYPE_DATA,
	AVMEDIA_TYPE_SUBTITLE,
};

enum AVCodecID {
	AV_CODEC_ID_NONE = 0,
	AV_CODEC_ID_H264 = 27,
	AV_CODEC_ID_HEVC = 173,
	AV_CODEC_ID_AAC = 86018,
};

enum AVRounding {
	AV_ROUND_ZERO = 0,
	AV_ROUND_INF = 1,
	AV_ROUND_DOWN = 2,
	AV_ROUND_UP = 3,
	AV_ROUND_NEAR_INF = 5,
	AV_ROUND_PASS_MINMAX = 8192,
};

typedef struct AVRational {
	int num;
	int den;
} AVRational;

enum AVChannelOrder {
	AV_CHANNEL_ORDER_UNSPEC,
	AV_CHANNEL_ORDER_NATIVE,
};

typedef struct AVChannelLayout {
	enum AVChannelOrder order;
	int nb_channels;
	union {
		uint64_t mask;
	} u;
	void *opaque;
} AVChannelLayout;

/* AV_CH_LAYOUT_4POINT1: front left, right, center, LFE, back center. */
#define AV_CHANNEL_LAYOUT_4POINT1 {AV_CHANNEL_ORDER_NATIVE, 5, {0x10Full}, NULL}

typedef struct AVDictionary AVDictionary;
typedef struct AVIOContext AVIOContext;
struct AVCodecTag;

typedef struct AVCodecParameters {
	enum AVMediaType codec_type;
	enum AVCodecID codec_id;
	uint32_t codec_tag;
	int sample_rate;
	int frame_size;
	AVChannelLayout ch_layout;
} AVCodecParameters;

typedef struct AVStream {
	int index;
	AVCodecParameters *codecpar;
	AVRational time_base;
	AVDictionary *metadata;
} AVStream;

typedef struct AVOutputFormat {
	int flags;
	const struct AVCodecTag *const *codec_tag;
} AVOutputFormat;

typedef struct AVInputFormat AVInputFormat;

typedef struct AVFormatContext {
	const AVOutputFormat *oformat;
	AVIOContext *pb;
	unsigned int nb_streams;
	AVStream **streams;
} AVFormatContext;

typedef struct AVPacket {
	int64_t pts;
	int64_t dts;
	int size;
	int stream_index;
	int64_t duration;
	int64_t pos;
} AVPacket;

int avformat_open_input(AVFormatContext **ps, const char *url, const AVInputFormat *fmt, AVDictionary **options);
int avformat_find_stream_info(AVFormatContext *ic, AVDictionary **options);
void av_dump_format(AVFormatContext *ic, int index, const char *url, int is_output);
int avformat_alloc_output_context2(AVFormatContext **ctx, const AVOutputFormat *oformat, const char *format_name,
				   const char *filename);
AVStream *avformat_new_stream(AVFormatContext *s, const void *c);
int avcodec_parameters_copy(AVCodecParameters *dst, const AVCodecParameters *src);
int av_dict_copy(AVDictionary **dst, const AVDictionary *src, int flags);
enum AVCodecID av_codec_get_id(const struct AVCodecTag *const *tags, unsigned int tag);
void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels);
int avio_open(AVIOContext **s, const char *url, int flags);
int avio_close(AVIOContext *s);
int avformat_write_header(AVFormatContext *s, AVDictionary **options);
int av_read_frame(AVFormatContext *s, AVPacket *pkt);
int av_interleaved_write_frame(AVFormatContext *s, AVPacket *pkt);
void av_packet_unref(AVPacket *pkt);
int av_write_trailer(AVFormatContext *s);
void avformat_close_input(AVFormatContext **s);
void avformat_free_context(AVFormatContext *s);
int av_get_audio_frame_duration2(AVCodecParameters *par, int frame_bytes);
int av_strerror(int errnum, char *errbuf, size_t errbuf_size);

int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding rnd);
int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq);
int64_t av_rescale_q_rnd(int64_t a, AVRational bq, AVRational cq, enum AVRounding rnd);
int64_t av_rescale_delta(AVRational in_tb, int64_t in_ts, AVRational fs_tb, int duration, int64_t *last,
			 AVRational out_tb);

static inline char *av_make_error_string(char *errbuf, size_t errbuf_size, int errnum)
{
	av_strerror(errnum, errbuf, errbuf_size);
	return errbuf;
}

#define av_err2str(errnum) av_make_error_string((char[AV_ERROR_MAX_STRING_SIZE]){0}, AV_ERROR_MAX_STRING_SIZE, errnum)
