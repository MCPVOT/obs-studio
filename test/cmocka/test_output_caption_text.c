/* obs_output_output_caption_text2() silently truncated every caption at
 * CAPTION_LINE_BYTES (128) bytes: caption_text_new() stored text in a fixed
 * 129-byte array, and the "split text into 32 character strings" comment was
 * never implemented. A caption longer than 128 bytes lost its tail, and a
 * multibyte UTF-8 character straddling byte 128 was cut in half, leaving
 * invalid UTF-8 on the wire.
 *
 * The fix implements the comment: text is word-wrapped at 32 columns and
 * paginated at 15 rows, each page enqueued as its own caption_text node with
 * the call's display_duration.
 *
 * The test registers a minimal video encoder and output, sets a video
 * encoder (which creates the caption track) and marks the output active so
 * the public API enqueues text. It then walks output->caption_tracks[0]'s
 * caption_text queue through obs-internal.h and checks the pages. No video
 * backend or encoder start is needed, so the test runs on every platform
 * that can obs_startup().
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include <obs.h>
#include <util/threading.h>

#include "obs-internal.h"

static int obs_up;

/* ------------------------------------------------------------------------- */

static const char *enc_name(void *type_data)
{
	(void)type_data;
	return "test encoder";
}

static void *enc_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	(void)settings;
	return encoder;
}

static void enc_destroy(void *data)
{
	(void)data;
}

static bool enc_encode(void *data, struct encoder_frame *frame, struct encoder_packet *packet,
		       bool *received_packet)
{
	(void)data;
	(void)frame;
	(void)packet;
	*received_packet = false;
	return true;
}

static const char *out_name(void *type_data)
{
	(void)type_data;
	return "test output";
}

static void *out_create(obs_data_t *settings, obs_output_t *output)
{
	(void)settings;
	return output;
}

static void out_destroy(void *data)
{
	(void)data;
}

static bool out_start(void *data)
{
	(void)data;
	return false;
}

static void out_stop(void *data, uint64_t ts)
{
	(void)data;
	(void)ts;
}

static void out_packet(void *data, struct encoder_packet *packet)
{
	(void)data;
	(void)packet;
}

static void register_types(void)
{
	struct obs_encoder_info enc_info = {
		.id = "test_caption_encoder",
		.type = OBS_ENCODER_VIDEO,
		.codec = "h264",
		.get_name = enc_name,
		.create = enc_create,
		.destroy = enc_destroy,
		.encode = enc_encode,
	};
	obs_register_encoder(&enc_info);

	struct obs_output_info out_info = {
		.id = "test_caption_output",
		.flags = OBS_OUTPUT_ENCODED | OBS_OUTPUT_VIDEO,
		.get_name = out_name,
		.create = out_create,
		.destroy = out_destroy,
		.start = out_start,
		.stop = out_stop,
		.encoded_packet = out_packet,
	};
	obs_register_output(&out_info);
}

/* ------------------------------------------------------------------------- */

struct caption_harness {
	obs_output_t *output;
	obs_encoder_t *encoder;
};

static struct caption_harness *harness_create(void)
{
	struct caption_harness *h = bzalloc(sizeof(*h));
	h->output = obs_output_create("test_caption_output", "test", NULL, NULL);
	h->encoder = obs_video_encoder_create("test_caption_encoder", "test enc", NULL, NULL);
	assert_non_null(h->output);
	assert_non_null(h->encoder);

	obs_output_set_video_encoder(h->output, h->encoder);
	assert_non_null(h->output->caption_tracks[0]);

	/* begin_data_capture would need a running core video; setting active
	 * is enough for obs_output_output_caption_text2() */
	os_atomic_set_bool(&h->output->active, true);
	return h;
}

static void harness_destroy(struct caption_harness *h)
{
	struct caption_track_data *ctrack = h->output->caption_tracks[0];

	pthread_mutex_lock(&ctrack->caption_mutex);
	struct caption_text *node = ctrack->caption_head;
	while (node) {
		struct caption_text *next = node->next;
		bfree(node);
		node = next;
	}
	ctrack->caption_head = NULL;
	ctrack->caption_tail = NULL;
	pthread_mutex_unlock(&ctrack->caption_mutex);

	os_atomic_set_bool(&h->output->active, false);
	obs_encoder_release(h->encoder);
	obs_output_release(h->output);
	bfree(h);
}

/* Collect the queued pages: returns the node count, fills `texts`/`durs`. */
static size_t collect_pages(struct caption_harness *h, const char **texts, double *durs, size_t max)
{
	struct caption_track_data *ctrack = h->output->caption_tracks[0];
	size_t count = 0;

	pthread_mutex_lock(&ctrack->caption_mutex);
	for (struct caption_text *node = ctrack->caption_head; node && count < max; node = node->next) {
		texts[count] = node->text;
		durs[count] = node->display_duration;
		count++;
	}
	pthread_mutex_unlock(&ctrack->caption_mutex);
	return count;
}

/* Every queued page must contain valid UTF-8: continuation bytes only after
 * a valid lead byte, and no truncated sequence at the end. */
static void assert_valid_utf8(const char *text)
{
	const unsigned char *c = (const unsigned char *)text;
	while (*c) {
		size_t len = 0;
		if (*c < 0x80)
			len = 1;
		else if ((*c & 0xE0) == 0xC0)
			len = 2;
		else if ((*c & 0xF0) == 0xE0)
			len = 3;
		else if ((*c & 0xF8) == 0xF0)
			len = 4;
		assert_true(len > 0);
		for (size_t i = 1; i < len; i++)
			assert_true((c[i] & 0xC0) == 0x80);
		c += len;
	}
}

/* Concatenate the pages, dropping the '\n' row separators, and compare
 * against the input text with its whitespace runs collapsed to single
 * spaces/newlines the same way the wrapper treats them. The call sites here
 * only need "every non-newline input byte survived, in order". */
static void assert_pages_preserve(struct caption_harness *h, const char *input)
{
	const char *texts[8];
	double durs[8];
	size_t pages = collect_pages(h, texts, durs, 8);

	size_t capacity = strlen(input) + 1;
	char *joined = bzalloc(capacity);
	size_t out = 0;
	for (size_t i = 0; i < pages; i++) {
		assert_valid_utf8(texts[i]);
		for (const char *c = texts[i]; *c && out < capacity - 1; c++) {
			if (*c != '\n')
				joined[out++] = *c;
		}
	}

	/* the inputs below use single-space-separated words, so the wrapper
	 * drops no input characters: removing whitespace from the input must
	 * reproduce the joined pages exactly */
	char *expected = bzalloc(capacity);
	size_t exp_len = 0;
	for (const char *c = input; *c; c++) {
		if (*c != '\n' && *c != ' ' && *c != '\r' && *c != '\t')
			expected[exp_len++] = *c;
	}
	assert_string_equal(expected, joined);
	bfree(expected);
	bfree(joined);
}

/* ------------------------------------------------------------------------- */

static char *repeat_char(size_t n, char c)
{
	char *s = bzalloc(n + 1);
	memset(s, c, n);
	return s;
}

static void caption_boundary_127(void **state)
{
	(void)state;
	struct caption_harness *h = harness_create();
	char *input = repeat_char(127, 'a');

	obs_output_output_caption_text2(h->output, input, 2.0);
	assert_pages_preserve(h, input);

	bfree(input);
	harness_destroy(h);
}

static void caption_boundary_128(void **state)
{
	(void)state;
	struct caption_harness *h = harness_create();
	char *input = repeat_char(128, 'a');

	obs_output_output_caption_text2(h->output, input, 2.0);
	assert_pages_preserve(h, input);

	bfree(input);
	harness_destroy(h);
}

static void caption_boundary_129(void **state)
{
	(void)state;
	struct caption_harness *h = harness_create();
	char *input = repeat_char(129, 'a');

	obs_output_output_caption_text2(h->output, input, 2.0);
	assert_pages_preserve(h, input);

	bfree(input);
	harness_destroy(h);
}

static void caption_multibyte_at_128(void **state)
{
	(void)state;
	struct caption_harness *h = harness_create();

	/* 127 'a' bytes, then U+00E9 (2 bytes) at bytes 128-129, then a tail:
	 * the old fixed 128-byte buffer cut the character in half. */
	char *input = bzalloc(256);
	memset(input, 'a', 127);
	strcat(input, "\xC3\xA9");
	strcat(input, "tail");

	obs_output_output_caption_text2(h->output, input, 2.0);
	assert_pages_preserve(h, input);

	const char *texts[8];
	double durs[8];
	size_t pages = collect_pages(h, texts, durs, 8);
	assert_true(pages > 0);
	/* the last row carries the tail that the 128-byte cap used to cut off */
	assert_non_null(strstr(texts[pages - 1], "tail"));

	bfree(input);
	harness_destroy(h);
}

static void caption_multi_page(void **state)
{
	(void)state;
	struct caption_harness *h = harness_create();

	/* 40 rows worth of text: more than the 15-row page, so the queue must
	 * hold ceil(40/15) = 3 pages, each with the call's duration */
	char input[2048] = {0};
	for (int i = 0; i < 40; i++) {
		char row[40];
		snprintf(row, sizeof(row), "row%02d-abcdefghijklmnopqrstuvwx", i);
		if (i)
			strcat(input, " ");
		strcat(input, row);
	}

	obs_output_output_caption_text2(h->output, input, 3.5);

	const char *texts[8];
	double durs[8];
	size_t pages = collect_pages(h, texts, durs, 8);
	assert_int_equal(3, pages);
	for (size_t i = 0; i < pages; i++)
		assert_true(durs[i] == 3.5);

	assert_non_null(strstr(texts[0], "row00"));
	assert_null(strstr(texts[0], "row15"));
	assert_non_null(strstr(texts[1], "row15"));
	assert_non_null(strstr(texts[2], "row39"));

	harness_destroy(h);
}

static void caption_word_wrap(void **state)
{
	(void)state;
	struct caption_harness *h = harness_create();

	/* words wrap at column 32: "wrapped" (7) does not fit after
	 * "aaaaaaaaaaaaaaaaaaaaaaaaa" (25) + a space, so it moves to row 2 */
	const char *input = "aaaaaaaaaaaaaaaaaaaaaaaaa wrapped";
	obs_output_output_caption_text2(h->output, input, 2.0);

	const char *texts[8];
	double durs[8];
	size_t pages = collect_pages(h, texts, durs, 8);
	assert_int_equal(1, pages);
	assert_string_equal("aaaaaaaaaaaaaaaaaaaaaaaaa\nwrapped", texts[0]);

	harness_destroy(h);
}

int main(void)
{
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		puts("obs_startup failed, skipping");
		return 0;
	}
	register_types();

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(caption_boundary_127),
		cmocka_unit_test(caption_boundary_128),
		cmocka_unit_test(caption_boundary_129),
		cmocka_unit_test(caption_multibyte_at_128),
		cmocka_unit_test(caption_multi_page),
		cmocka_unit_test(caption_word_wrap),
	};
	int rc = cmocka_run_group_tests(tests, NULL, NULL);

	obs_shutdown();
	return rc;
}
