/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <linux/videodev2.h>
#include <obs-module.h>

#include "v4l2-decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What the capture thread does with one dequeued buffer. */
enum v4l2_frame_action {
	/* out holds a picture for obs_source_output_video() */
	V4L2_FRAME_OUTPUT,
	/* nothing to show for this buffer; requeue it */
	V4L2_FRAME_SKIP,
	/* the buffer could not be decoded; requeue it, the decoder takes the
	 * next one as usual */
	V4L2_FRAME_DROP,
};

/**
 * Decide what to do with a dequeued buffer, decoding MJPEG and H.264 into
 * out and pointing out's planes into the buffer for raw formats.
 *
 * No buffer content ends the capture: a damaged frame only loses that
 * frame. Only device errors (select, VIDIOC_DQBUF, VIDIOC_QBUF) stop it.
 *
 * @param out the obs frame prepared for this capture
 * @param pixfmt the V4L2 pixel format of the capture
 * @param buf the buffer as returned by VIDIOC_DQBUF
 * @param start the mapped memory of the buffer
 * @param plane_offsets plane offsets of raw formats in the buffer
 * @param decoder the decoder for MJPEG and H.264
 */
enum v4l2_frame_action v4l2_frame_action(struct obs_source_frame *out, int pixfmt, const struct v4l2_buffer *buf,
					 uint8_t *start, const size_t *plane_offsets, struct v4l2_decoder *decoder);

#ifdef __cplusplus
}
#endif
