#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Reject a device stream format that reports no channels, and clamp the channel
 * count a device does report to what the plugin can handle.
 *
 * CoreAudio answers kAudioUnitProperty_StreamFormat with noErr for a device that
 * has no capture stream (any pure output device while it is the system default
 * output) and reports zero channels for it. The caller used to take that count
 * straight into bzalloc(channels * sizeof(char *)), and libobs turns a zero-size
 * allocation into bcrash(), so the whole application died while the scene
 * collection was being loaded. Zero channels means "nothing to capture": fail
 * initialization instead, which the reconnect thread retries with backoff.
 *
 * Returns false (leaving *channels_out untouched) when the format cannot be
 * captured, true with the clamped count in *channels_out otherwise.
 */
static inline bool coreaudio_capture_format(uint32_t channels_per_frame, uint32_t max_channels, uint32_t *channels_out)
{
	if (!channels_per_frame || !max_channels) {
		return false;
	}

	*channels_out = channels_per_frame > max_channels ? max_channels : channels_per_frame;
	return true;
}
