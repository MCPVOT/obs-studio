#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "libobs/util/bmem-windows.h"
#include "libobs/util/config-file-windows.h"
#include "libobs/media-io/audio-io-windows.h"
#include "libobs/util/base-windows.h"
#include "libobs/util/utf8-windows.h"

/*
 * The libobs C reference's Windows-only choices (obs-rust/obs-studio#876)
 * live in self-guarded Windows headers: off Windows they define nothing, so
 * bmem keeps its alignment hack, config files get no UTF-8 BOM and the audio
 * thread asks for no MMCSS class, base keeps its printf checks and hidden
 * symbols, and utf8 keeps its portable converter. The Windows side is compiled by the cross
 * check.
 */

static void test_nothing_off_windows(void **state)
{
	(void)state;
	int defined = 0;
#ifdef ALIGNED_MALLOC
	defined |= 1;
#endif
#ifdef CONFIG_FILE_BOM
	defined |= 2;
#endif
#ifdef AUDIO_IO_HOST_THREAD
	defined |= 4;
#endif
#if defined(BASE_HOST_VPRINTFATTR) || defined(BASE_HIDDEN)
	defined |= 8;
#endif
#ifdef UTF8_HOST_WINDOWS
	defined |= 16;
#endif
	assert_int_equal(defined, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_nothing_off_windows),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
