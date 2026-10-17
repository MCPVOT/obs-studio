#include "plugins/obs-qsv11/build/obs-qsv11-linux-c.pch.h"

bool qsv_host_texture_valid(const struct encoder_texture *tex)
{
	return tex && tex->tex[0] && tex->tex[1];
}
