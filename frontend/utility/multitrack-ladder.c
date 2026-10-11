#include "multitrack-ladder.h"

#include <string.h>

void mtv_ladder_fill(const struct mtv_ladder_in *in, struct mtv_ladder_out *out)
{
	(void)in;

	if (!out)
		return;

	out->row_count = 0;
	out->total_bitrate_kbps = 0;
	out->any_bitrate = 0;
	out->audio_count = 0;
	out->config_id[0] = '\0';
}
