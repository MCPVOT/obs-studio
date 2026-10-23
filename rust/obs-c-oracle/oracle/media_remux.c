/* Test-only oracle: the original libobs/media-io/media-remux.c.h, unmodified,
 * with its exports renamed to oracle_* so it can link next to the Rust
 * implementation. Its group's precompiled header finds the FFmpeg stand-in
 * (ffmpeg-shim/) instead of FFmpeg, and blog is renamed to the stand-in's
 * recorder, so its calls and log lines land in one log
 * (media_remux_ffmpeg.c). */
#define blog oracle_remux_blog
#define media_remux_job_create oracle_media_remux_job_create
#define media_remux_job_process oracle_media_remux_job_process
#define media_remux_job_destroy oracle_media_remux_job_destroy

#include "media-io/media-remux.c.h"
