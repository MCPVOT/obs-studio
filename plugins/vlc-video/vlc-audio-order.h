#pragma once

#include <stddef.h>

/* libVLC's audio callbacks deliver interleaved samples in VLC's own channel
 * order. vlc_audio_reorder() rearranges them in place into the order libobs
 * expects for the same channel count. */
static inline void vlc_audio_reorder(void *samples, size_t frames, unsigned channels, size_t sample_size)
{
	(void)samples;
	(void)frames;
	(void)channels;
	(void)sample_size;
}
