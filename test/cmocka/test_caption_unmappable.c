/* One character EIA-608 cannot encode must not drop the rest of its caption
 * row (obs-rust/obs-studio#376). obs-output.c emits caption text with
 * caption_frame_from_text() and sei_from_caption_frame(); a decoder sees what
 * sei_to_caption_frame() reads back. "A中B" used to arrive as "A". */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

#include <caption/caption.h>
#include <caption/mpeg.h>

/* Encodes text the way obs-output.c does and returns what a decoder reads. */
static void round_trip(const char *text, char *out, size_t out_size)
{
	caption_frame_t in;
	caption_frame_init(&in);
	caption_frame_from_text(&in, (const utf8_char_t *)text);

	sei_t sei;
	sei_init(&sei, 0.0);
	sei_from_caption_frame(&sei, &in);

	caption_frame_t decoded;
	caption_frame_init(&decoded);
	sei_to_caption_frame(&sei, &decoded);
	sei_free(&sei);

	char buf[CAPTION_FRAME_TEXT_BYTES];
	size_t len = caption_frame_to_text(&decoded, (utf8_char_t *)buf);
	if (len >= out_size)
		len = out_size - 1;
	memcpy(out, buf, len);
	out[len] = '\0';
}

static void mappable_text_round_trips(void **state)
{
	(void)state;
	char out[CAPTION_FRAME_TEXT_BYTES];
	round_trip("AB", out, sizeof(out));
	assert_string_equal(out, "AB");
}

static void unmappable_char_keeps_rest_of_row(void **state)
{
	(void)state;
	char out[CAPTION_FRAME_TEXT_BYTES];
	round_trip("A\xe4\xb8\xad"
		   "B",
		   out, sizeof(out)); /* "A中B" */
	if (!strchr(out, 'B')) {
		fprintf(stderr, "\"A\\u4e2dB\" decoded as \"%s\"\n", out);
		fail();
	}
	assert_int_equal(out[0], 'A');
}

static void emoji_keeps_rest_of_row(void **state)
{
	(void)state;
	char out[CAPTION_FRAME_TEXT_BYTES];
	round_trip("Hi \xf0\x9f\xa6\x84 there", out, sizeof(out)); /* "Hi 🦄 there" */
	if (!strstr(out, "there")) {
		fprintf(stderr, "\"Hi \\U0001f984 there\" decoded as \"%s\"\n", out);
		fail();
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(mappable_text_round_trips),
		cmocka_unit_test(unmappable_char_keeps_rest_of_row),
		cmocka_unit_test(emoji_keeps_rest_of_row),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
