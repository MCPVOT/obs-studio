/* ffmpeg_video_encoder_init() (plugins/obs-ffmpeg/obs-ffmpeg-video-encoders.c.h)
 * builds "Couldn't find encoder: <name>" when FFmpeg lacks the encoder. It
 * printed into an uninitialized `struct dstr`, so dstr_printf() reallocated
 * whatever pointer was left on the stack (split from #1464).
 *
 * dstr_printf() is replaced for the included file: it records whether the
 * dstr it got was zeroed, then zeroes it and formats for real, so the
 * check fails cleanly instead of corrupting the heap. The stack is filled
 * with a nonzero pattern right before the call, so leftover stack contents
 * are deterministic for the uninitialized case.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include "plugins/obs-ffmpeg/build/obs-ffmpeg-video-encoders.pch.h"

static bool printf_saw_garbage;
static int printf_calls;
static char last_error[256];

static void test_dstr_printf(struct dstr *dst, const char *format, ...)
{
	printf_calls++;
	if (dst->array || dst->len || dst->capacity)
		printf_saw_garbage = true;
	memset(dst, 0, sizeof(*dst));

	va_list args;
	va_start(args, format);
	dstr_vprintf(dst, format, args);
	va_end(args);
}

static void test_set_last_error(obs_encoder_t *encoder, const char *message)
{
	(void)encoder;
	snprintf(last_error, sizeof(last_error), "%s", message ? message : "");
}

static const char *test_encoder_get_name(const obs_encoder_t *encoder)
{
	(void)encoder;
	return "test";
}

const char *obs_module_text(const char *text)
{
	return text;
}

#define dstr_printf test_dstr_printf
#define obs_encoder_set_last_error test_set_last_error
#define obs_encoder_get_name test_encoder_get_name
#include "plugins/obs-ffmpeg/obs-ffmpeg-video-encoders.c.h"
#undef obs_encoder_get_name
#undef obs_encoder_set_last_error
#undef dstr_printf

static __attribute__((noinline)) void dirty_stack(void)
{
	volatile unsigned char junk[16384];
	for (size_t i = 0; i < sizeof(junk); i++)
		junk[i] = 0xA5;
}

static __attribute__((noinline)) bool init_missing(struct ffmpeg_video_encoder *enc)
{
	return ffmpeg_video_encoder_init(enc, NULL, NULL, "obs_rust_no_such_encoder", NULL, "missing", NULL, NULL);
}

static void test_missing_encoder_error(void **state)
{
	(void)state;
	struct ffmpeg_video_encoder enc = {0};

	dirty_stack();
	assert_false(init_missing(&enc));

	assert_int_equal(printf_calls, 1);
	assert_false(printf_saw_garbage);
	assert_string_equal(last_error, "Couldn't find encoder: obs_rust_no_such_encoder");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_missing_encoder_error),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
