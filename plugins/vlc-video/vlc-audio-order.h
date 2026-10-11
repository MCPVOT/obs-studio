#pragma once

#include <stddef.h>
#include <string.h>

/* libVLC's audio callbacks deliver interleaved samples in VLC's own channel
 * order: L R [SL SR] RL RR C LFE. libobs expects FL FR FC LFE RL RR for 5.1
 * and FL FR FC LFE RL RR SL SR for 7.1. vlc_audio_reorder() rearranges the
 * samples in place; other channel counts are left untouched. */
static inline void vlc_audio_reorder(void *samples, size_t frames, unsigned channels, size_t sample_size)
{
	/* libobs channel i takes VLC channel order[i] */
	static const unsigned char order_5_1[6] = {0, 1, 4, 5, 2, 3};
	static const unsigned char order_7_1[8] = {0, 1, 6, 7, 4, 5, 2, 3};
	const unsigned char *order;
	unsigned char frame[8 * 8];
	unsigned char *data = samples;

	if (channels == 6)
		order = order_5_1;
	else if (channels == 8)
		order = order_7_1;
	else
		return;

	if (!samples || sample_size > 8)
		return;

	const size_t frame_size = channels * sample_size;
	for (size_t f = 0; f < frames; f++, data += frame_size) {
		memcpy(frame, data, frame_size);
		for (unsigned ch = 0; ch < channels; ch++)
			memcpy(data + ch * sample_size, frame + order[ch] * sample_size, sample_size);
	}
}
