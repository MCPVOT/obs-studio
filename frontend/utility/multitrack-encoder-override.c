#include "multitrack-encoder-override.h"

#include <string.h>

void mtv_translate_encoder_override(const char *encoder_id, const struct mtv_override_in *in,
				    struct mtv_override_out *out)
{
	(void)encoder_id;
	(void)in;
	if (!out)
		return;
	memset(out, 0, sizeof(*out));
}

bool mtv_filter_lookahead_opts(const char *opts, bool disable_lookahead, char *dst, size_t dst_size)
{
	(void)opts;
	(void)disable_lookahead;
	if (!dst || dst_size == 0)
		return false;
	dst[0] = '\0';
	return false;
}
