#include "slideshow-size.h"

#include <inttypes.h>
#include <stdio.h>

void slideshow_parse_size(const char *str, uint32_t base_cx, uint32_t base_cy, uint32_t *cx, uint32_t *cy)
{
	(void)base_cx;
	(void)base_cy;

	if (!str || sscanf(str, "%" SCNu32 "x%" SCNu32, cx, cy) != 2) {
		*cx = 0;
		*cy = 0;
	}
}
