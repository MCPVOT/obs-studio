#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
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
			assert_int_equal(AMF_DecodeInt16(data),
					 (uint16_t)byte_values[byte] << ((1 - position) * 8));
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
			assert_int_equal(AMF_DecodeInt24(data),
					 (uint32_t)byte_values[byte] << ((2 - position) * 8));
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
			assert_int_equal(AMF_DecodeInt32(data),
					 (uint32_t)byte_values[byte] << ((3 - position) * 8));
		}
	}
}

static void assert_decode_int32le(const char data[4], uint32_t expected)
{
	RTMP *rtmp = RTMP_Alloc();
	assert_non_null(rtmp);
	RTMP_Init(rtmp);

	char header[] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
			 0x00, 0x14, data[0], data[1], data[2], data[3]};
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

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(decode_int16_bytes),
		cmocka_unit_test(decode_int24_bytes),
		cmocka_unit_test(decode_int32le_bytes),
		cmocka_unit_test(decode_int32_bytes),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
