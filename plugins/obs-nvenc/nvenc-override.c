#include "nvenc-override.h"

bool nvenc_lookahead_enabled(bool preset_enables, bool explicit_setting, bool setting_on)
{
	(void)preset_enables;
	(void)explicit_setting;
	(void)setting_on;
	return false;
}

bool nvenc_lookahead_force_off(bool explicit_setting, bool setting_on)
{
	(void)explicit_setting;
	(void)setting_on;
	return false;
}

int nvenc_bframes_for_uhq(bool uhq, int bframes)
{
	(void)uhq;
	(void)bframes;
	return 0;
}
