#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include <librtmp/amf.h>
#include <librtmp/rtmp.h>

static void decode_int16_bytes(void **state)
{
	(void)state;
	const uint8_t byte_values[] = {0x80, 0xff};

	assert_int_equal(AMF_DecodeInt16("\x00\x00"), 0x0000);
	assert_int_equal(AMF_DecodeInt16("\x7f\xff"), 0x7fff);
	assert_int_equal(AMF_DecodeInt16("\x80\x00"), 0x8000);
	assert_int_equal(AMF_DecodeInt16("\xff\xff"), 0xffff);
	assert_int_equal(AMF_DecodeInt16("\x12\x34"), 0x1234);

	for (size_t byte = 0; byte < sizeof(byte_values); byte++) {
		for (size_t position = 0; position < 2; position++) {
			char data[2] = {0};
			data[position] = (char)byte_values[byte];
			assert_int_equal(AMF_DecodeInt16(data), (uint16_t)byte_values[byte] << ((1 - position) * 8));
		}
	}
}

static void decode_int24_bytes(void **state)
{
	(void)state;
	const uint8_t byte_values[] = {0x80, 0xff};

	assert_int_equal(AMF_DecodeInt24("\x00\x00\x00"), 0x000000);
	assert_int_equal(AMF_DecodeInt24("\x7f\xff\xff"), 0x7fffff);
	assert_int_equal(AMF_DecodeInt24("\x80\x00\x00"), 0x800000);
	assert_int_equal(AMF_DecodeInt24("\xff\xff\xff"), 0xffffff);
	assert_int_equal(AMF_DecodeInt24("\x12\x34\x56"), 0x123456);

	for (size_t byte = 0; byte < sizeof(byte_values); byte++) {
		for (size_t position = 0; position < 3; position++) {
			char data[3] = {0};
			data[position] = (char)byte_values[byte];
			assert_int_equal(AMF_DecodeInt24(data), (uint32_t)byte_values[byte] << ((2 - position) * 8));
		}
	}
}

static void decode_int32_bytes(void **state)
{
	(void)state;
	const uint8_t byte_values[] = {0x80, 0xff};
	const struct {
		char data[4];
		uint32_t expected;
	} cases[] = {
		{{0x00, 0x00, 0x00, 0x00}, UINT32_C(0x00000000)},
		{{0x7f, (char)0xff, (char)0xff, (char)0xff}, UINT32_C(0x7fffffff)},
		{{(char)0x80, 0x00, 0x00, 0x00}, UINT32_C(0x80000000)},
		{{(char)0xff, (char)0xff, (char)0xff, (char)0xff}, UINT32_C(0xffffffff)},
		{{0x12, 0x34, 0x56, 0x78}, UINT32_C(0x12345678)},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		assert_int_equal(AMF_DecodeInt32(cases[i].data), cases[i].expected);

	for (size_t byte = 0; byte < sizeof(byte_values); byte++) {
		for (size_t position = 0; position < 4; position++) {
			char data[4] = {0};
			data[position] = (char)byte_values[byte];
			assert_int_equal(AMF_DecodeInt32(data), (uint32_t)byte_values[byte] << ((3 - position) * 8));
		}
	}
}

static void assert_decode_int32le(const char data[4], uint32_t expected)
{
	RTMP *rtmp = RTMP_Alloc();
	assert_non_null(rtmp);
	RTMP_Init(rtmp);

	char header[] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, data[0], data[1], data[2], data[3]};
	memcpy(rtmp->m_sb.sb_buf, header, sizeof(header));
	rtmp->m_sb.sb_start = rtmp->m_sb.sb_buf;
	rtmp->m_sb.sb_size = sizeof(header);

	RTMPPacket packet = {0};
	assert_true(RTMP_ReadPacket(rtmp, &packet));
	assert_int_equal((uint32_t)packet.m_nInfoField2, expected);

	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
}

static void decode_int32le_bytes(void **state)
{
	(void)state;
	const uint8_t byte_values[] = {0x80, 0xff};
	const struct {
		char data[4];
		uint32_t expected;
	} cases[] = {
		{{0x00, 0x00, 0x00, 0x00}, UINT32_C(0x00000000)},
		{{(char)0xff, (char)0xff, (char)0xff, 0x7f}, UINT32_C(0x7fffffff)},
		{{0x00, 0x00, 0x00, (char)0x80}, UINT32_C(0x80000000)},
		{{(char)0xff, (char)0xff, (char)0xff, (char)0xff}, UINT32_C(0xffffffff)},
		{{0x78, 0x56, 0x34, 0x12}, UINT32_C(0x12345678)},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		assert_decode_int32le(cases[i].data, cases[i].expected);

	for (size_t byte = 0; byte < sizeof(byte_values); byte++) {
		for (size_t position = 0; position < 4; position++) {
			char data[4] = {0};
			data[position] = (char)byte_values[byte];
			assert_decode_int32le(data, (uint32_t)byte_values[byte] << (position * 8));
		}
	}
}

static void client_packet_decodes_flv_timestamp(void **state)
{
	(void)state;
	char body[] = {
		0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (char)0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b,
	};
	RTMP *rtmp = RTMP_Alloc();
	assert_non_null(rtmp);
	RTMP_Init(rtmp);

	RTMPPacket packet = {0};
	packet.m_packetType = RTMP_PACKET_TYPE_FLASH_VIDEO;
	packet.m_nBodySize = sizeof(body);
	packet.m_body = body;
	assert_int_equal(RTMP_ClientPacket(rtmp, &packet), 1);
	assert_int_equal(rtmp->m_mediaStamp, UINT32_C(0x80000000));

	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
}

static void read_decodes_flv_timestamps(void **state)
{
	(void)state;
	const char packet[] = {
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00,       0x0f, 0x16, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, (char)0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b,
	};
	char output[sizeof(packet)] = {0};
	char initial_frame = 0;
	RTMP *rtmp = RTMP_Alloc();
	assert_non_null(rtmp);
	RTMP_Init(rtmp);
	rtmp->m_sb.sb_socket = 0;
	memcpy(rtmp->m_sb.sb_buf, packet, sizeof(packet));
	rtmp->m_sb.sb_start = rtmp->m_sb.sb_buf;
	rtmp->m_sb.sb_size = sizeof(packet);
	rtmp->m_read.flags = RTMP_READ_HEADER | RTMP_READ_RESUME | RTMP_READ_GOTFLVK;
	rtmp->m_read.initialFrameType = RTMP_PACKET_TYPE_VIDEO;
	rtmp->m_read.initialFrame = &initial_frame;
	rtmp->m_read.nInitialFrameSize = 1;
	rtmp->m_read.nResumeTS = UINT32_C(0x7fffffff);

	assert_int_equal(RTMP_Read(rtmp, output, sizeof(output)), 15);
	assert_int_equal(rtmp->m_mediaStamp, UINT32_C(0x80000000));
	assert_int_equal(rtmp->m_read.timestamp, UINT32_C(0x7fffffff));
	assert_memory_equal(output + 4, "\xff\xff\xff\x7f", 4);

	rtmp->m_sb.sb_socket = -1;
	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
}

static void write_decodes_flv_timestamp(void **state)
{
	(void)state;
	const char tag[] = {
		0x09, 0x00, 0x00, 0x02, 0x12, 0x34, 0x56, (char)0x80, 0x00, 0x00, 0x00, 0x00,
	};
	RTMP *rtmp = RTMP_Alloc();
	assert_non_null(rtmp);
	RTMP_Init(rtmp);

	assert_int_equal(RTMP_Write(rtmp, tag, sizeof(tag), 0), sizeof(tag));
	assert_int_equal(rtmp->m_write.m_nTimeStamp, UINT32_C(0x80123456));

	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
}

static int discard_rtmp_output(RTMPSockBuf *sockbuf, const char *buffer, int length, void *context)
{
	(void)sockbuf;
	(void)buffer;
	(*(int *)context)++;
	return length;
}

static char g_last_sent[512];
static int g_last_sent_len;

static int capture_rtmp_output(RTMPSockBuf *sockbuf, const char *buffer, int length, void *context)
{
	(void)sockbuf;
	(*((int *)context))++;
	/* the decoded token is echoed back in the secureToken response, so the
	 * sent bytes pin the decoder's output */
	if (length > 0 && length <= (int)sizeof(g_last_sent)) {
		memcpy(g_last_sent, buffer, (size_t)length);
		g_last_sent_len = length;
	}
	return length;
}

/* Drive RTMP_ClientPacket by the real path: HandleInvoke finds the
 * server's secureToken property and calls DecodeTEA on it, then echoes
 * it back. `key` is r->Link.token (the TEA key) and `text` is the
 * server-controlled secureToken value. */
static void run_secure_token_case(const char *text, const char *key)
{
	AVal result = AVC("_result");
	AVal secure_token = AVC("secureToken");
	AVal response = {(char *)text, (int)strlen(text)};
	char body[256];
	char *end = body;
	int send_count = 0;

	g_last_sent_len = 0;
	memset(g_last_sent, 0, sizeof(g_last_sent));

	end = AMF_EncodeString(end, body + sizeof(body), &result);
	end = AMF_EncodeNumber(end, body + sizeof(body), 1.0);
	*end++ = AMF_NULL;
	*end++ = AMF_OBJECT;
	end = AMF_EncodeNamedString(end, body + sizeof(body), &secure_token, &response);
	*end++ = 0;
	*end++ = 0;
	*end++ = AMF_OBJECT_END;

	RTMP *rtmp = RTMP_Alloc();
	assert_non_null(rtmp);
	RTMP_Init(rtmp);
	rtmp->Link.token.av_val = (char *)key;
	rtmp->Link.token.av_len = (int)strlen(key);
	rtmp->Link.protocol |= RTMP_FEATURE_WRITE;
	rtmp->m_methodCalls = calloc(1, sizeof(*rtmp->m_methodCalls));
	assert_non_null(rtmp->m_methodCalls);
	rtmp->m_methodCalls[0].name.av_val = malloc(sizeof("connect"));
	assert_non_null(rtmp->m_methodCalls[0].name.av_val);
	memcpy(rtmp->m_methodCalls[0].name.av_val, "connect", sizeof("connect"));
	rtmp->m_methodCalls[0].name.av_len = sizeof("connect") - 1;
	rtmp->m_methodCalls[0].num = 1;
	rtmp->m_numCalls = 1;
	rtmp->m_bCustomSend = TRUE;
	rtmp->m_customSendFunc = capture_rtmp_output;
	rtmp->m_customSendParam = &send_count;

	RTMPPacket packet = {0};
	packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
	packet.m_nBodySize = (uint32_t)(end - body);
	packet.m_body = body;
	RTMP_ClientPacket(rtmp, &packet);
	assert_int_equal(send_count, 1);

	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
}

/* Server-sent secureToken text with a high top byte (rtmp.c DecodeTEA hex unpack). */
static void secure_token_decode_uses_unsigned_shift(void **state)
{
	(void)state;
	run_secure_token_case("00000080", "0123456789abcdef");
}

/* Configured token (TEA key) with a high byte in the top position of a packed word. */
static void secure_token_key_uses_unsigned_shift(void **state)
{
	(void)state;
	run_secure_token_case("00000080", "012\x80"
					  "456789abcdef");
}

/* The secureToken text is server-controlled. DecodeTEA unpacks 8 hex
 * characters per 32-bit word, so a well-formed token is a non-empty
 * multiple of 8 characters. An empty one makes n = 0, which reads
 * v[n-1] before the buffer and divides by zero; a length that is not a
 * multiple of 8 rounds n up and reads past the token. Both must be
 * rejected instead of decoded. */
static void secure_token_short_or_unaligned_is_rejected(void **state)
{
	(void)state;
	/* the response echoes the token back. DecodeTEA halves av_len when it
	 * decodes, so the echoed length pins "did not decode": without the
	 * length guard these decode anyway and the lengths change. */
	run_secure_token_case("abc", "0123456789abcdef");
	assert_int_equal(g_last_sent_len, 46);
	/* the echoed token keeps its original 3 bytes: no decode happened */
	assert_memory_equal(g_last_sent + 43, "abc", 3);

	run_secure_token_case("1234567", "0123456789abcdef");
	assert_int_equal(g_last_sent_len, 50);
	assert_memory_equal(g_last_sent + 43, "1234567", 7);

	run_secure_token_case("123456789", "0123456789abcdef");
	assert_int_equal(g_last_sent_len, 52);
	assert_memory_equal(g_last_sent + 43, "123456789", 9);

	/* the empty token is the one that reads v[n-1] before its buffer and
	 * divides by zero on master */
	run_secure_token_case("", "0123456789abcdef");
	assert_int_equal(g_last_sent_len, 43);
	assert_int_equal(g_last_sent[43], 0);
}

/* Valid tokens still decode, and decode to the same bytes as before. */
static void secure_token_valid_lengths_still_decode(void **state)
{
	(void)state;
	run_secure_token_case("01234567", "0123456789abcdef");
	assert_int_equal(g_last_sent_len, 47);
	/* the echoed secureTokenResponse, which carries the 4 decoded bytes
	 * at the end: \xc0\xa9\x0a\xdc. Pinned so a change in the decoder's
	 * output is caught, not just "it did not crash". */
	assert_memory_equal(g_last_sent,
			    "\x43\x00\x00\x00\x00\x00\x27\x14"
			    "\x02\x00\x13"
			    "secureTokenResponse"
			    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05"
			    "\x02\x00\x04"
			    "\xc0\xa9\x0a\xdc",
			    47);
	run_secure_token_case("0123456789abcdef", "0123456789abcdef");
	assert_int_equal(g_last_sent_len, 51);
	assert_memory_equal(g_last_sent,
			    "\x43\x00\x00\x00\x00\x00\x2b\x14"
			    "\x02\x00\x13"
			    "secureTokenResponse"
			    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05"
			    "\x02\x00\x08"
			    "\x5e\x6a\x95\x67\x90\xd9\xde\x45",
			    51);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(decode_int16_bytes),
		cmocka_unit_test(decode_int24_bytes),
		cmocka_unit_test(decode_int32le_bytes),
		cmocka_unit_test(decode_int32_bytes),
		cmocka_unit_test(client_packet_decodes_flv_timestamp),
		cmocka_unit_test(read_decodes_flv_timestamps),
		cmocka_unit_test(write_decodes_flv_timestamp),
		cmocka_unit_test(secure_token_decode_uses_unsigned_shift),
		cmocka_unit_test(secure_token_key_uses_unsigned_shift),
		cmocka_unit_test(secure_token_short_or_unaligned_is_rejected),
		cmocka_unit_test(secure_token_valid_lengths_still_decode),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
