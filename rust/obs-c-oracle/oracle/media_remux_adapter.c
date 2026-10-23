/* Test-only build of the private adapter the Rust media-remux port calls
 * (libobs/media-io/media-remux-ffmpeg.c.h, copied after its precompiled
 * header by build.rs), over the same FFmpeg stand-in and log recorder as the
 * original C (media_remux.c), so Tier 3 compares the calls both make. */
#define blog oracle_remux_blog

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/c99defs.h"
#include "util/base.h"
#include "util/bmem.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>

#include "media-remux-ffmpeg.inc"

size_t oracle_media_remux_adapter_layout(size_t *out);

/* size, align, then every field offset of struct obs_media_remux_stream and
 * struct obs_media_remux_packet, in declaration order. Returns how many
 * values it wrote. */
size_t oracle_media_remux_adapter_layout(size_t *out)
{
	size_t n = 0;

	out[n++] = sizeof(struct obs_media_remux_stream);
	out[n++] = _Alignof(struct obs_media_remux_stream);
	out[n++] = offsetof(struct obs_media_remux_stream, codec_type);
	out[n++] = offsetof(struct obs_media_remux_stream, codec_id);
	out[n++] = offsetof(struct obs_media_remux_stream, sample_rate);
	out[n++] = offsetof(struct obs_media_remux_stream, frame_size);
	out[n++] = offsetof(struct obs_media_remux_stream, channels);
	out[n++] = offsetof(struct obs_media_remux_stream, time_base);
	out[n++] = sizeof(struct obs_media_remux_packet);
	out[n++] = _Alignof(struct obs_media_remux_packet);
	out[n++] = offsetof(struct obs_media_remux_packet, pts);
	out[n++] = offsetof(struct obs_media_remux_packet, dts);
	out[n++] = offsetof(struct obs_media_remux_packet, duration);
	out[n++] = offsetof(struct obs_media_remux_packet, pos);
	out[n++] = offsetof(struct obs_media_remux_packet, size);
	out[n++] = offsetof(struct obs_media_remux_packet, stream_index);
	return n;
}
