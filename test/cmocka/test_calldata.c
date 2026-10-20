#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <callback/calldata.h>
#include <util/base.h>
#include <util/bmem.h>

/* Characterization tests: the expected values are what
 * libobs/callback/calldata.c.h produces today (#1439). Cases marked
 * "characterized, not endorsed" pin odd behavior of that C. */

struct log_record {
	int calls;
	int level;
	char message[128];
};

static void capture_log(int level, const char *format, va_list args, void *param)
{
	struct log_record *rec = param;

	rec->calls++;
	rec->level = level;
	vsnprintf(rec->message, sizeof(rec->message), format, args);
}

static size_t read_size(const uint8_t *p)
{
	size_t value = 0;
	memcpy(&value, p, sizeof(value));
	return value;
}

/* name length, name, data length, data, and the terminating 0. */
static size_t param_bytes(size_t name_len, size_t data_size)
{
	return sizeof(size_t) * 3 + name_len + data_size;
}

static void expect_param(const struct calldata *cd, size_t *offset, const char *name, const void *bytes, size_t len)
{
	const uint8_t *p = cd->stack + *offset;
	size_t name_len = strlen(name) + 1;

	assert_int_equal(read_size(p), name_len);
	assert_memory_equal(p + sizeof(size_t), name, name_len);
	p += sizeof(size_t) + name_len;
	assert_int_equal(read_size(p), len);
	if (len)
		assert_memory_equal(p + sizeof(size_t), bytes, len);
	*offset += sizeof(size_t) + name_len + sizeof(size_t) + len;
}

static void expect_end(const struct calldata *cd, size_t offset)
{
	assert_int_equal(read_size(cd->stack + offset), 0);
	assert_int_equal(cd->size, offset + sizeof(size_t));
}

static void guards_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	struct calldata cd;
	long long out = 0x1111;
	const char *str = (const char *)(uintptr_t)1;

	calldata_init(&cd);
	assert_null(cd.stack);
	assert_int_equal(cd.size, 0);
	assert_int_equal(cd.capacity, 0);
	assert_false(cd.fixed);

	assert_false(calldata_get_data(NULL, "a", &out, sizeof(out)));
	assert_false(calldata_get_data(&cd, NULL, &out, sizeof(out)));
	assert_false(calldata_get_data(&cd, "", &out, sizeof(out)));
	assert_false(calldata_get_string(NULL, "a", &str));
	assert_false(calldata_get_string(&cd, NULL, &str));
	assert_false(calldata_get_string(&cd, "", &str));
	assert_false(calldata_get_string(&cd, "missing", &str));
	assert_int_equal(out, 0x1111);
	assert_ptr_equal(str, (const char *)(uintptr_t)1);

	calldata_set_data(NULL, "a", &out, sizeof(out));
	calldata_set_data(&cd, NULL, &out, sizeof(out));
	calldata_set_data(&cd, "", &out, sizeof(out));
	assert_null(cd.stack);
	assert_int_equal(bnum_allocs(), allocs);

	calldata_free(&cd);
	assert_int_equal(bnum_allocs(), allocs);
}

static void first_param_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	struct calldata cd;
	long long value = 42;
	long long out = 0;
	uint8_t raw[sizeof(value) + 1];
	size_t offset = 0;

	calldata_init(&cd);
	calldata_set_int(&cd, "a", value);

	assert_int_equal(bnum_allocs(), allocs + 1);
	assert_non_null(cd.stack);
	assert_false(cd.fixed);
	/* The first allocation is at least 128 bytes. The used size is not. */
	assert_int_equal(cd.capacity, 128);
	assert_int_equal(cd.size, param_bytes(2, sizeof(value)));
	assert_true(cd.size < cd.capacity);

	expect_param(&cd, &offset, "a", &value, sizeof(value));
	expect_end(&cd, offset);

	assert_true(calldata_get_int(&cd, "a", &out));
	assert_int_equal(out, value);
	assert_false(calldata_get_int(&cd, "A", &out));
	assert_false(calldata_get_int(&cd, "b", &out));

	/* Name "a" leaves the payload off a size_t boundary. memcpy still
	 * round-trips it, including into an unaligned destination. */
	assert_true((sizeof(size_t) + 2) % sizeof(size_t) != 0);
	memset(raw, 0x5A, sizeof(raw));
	assert_true(calldata_get_data(&cd, "a", raw + 1, sizeof(value)));
	assert_memory_equal(raw + 1, &value, sizeof(value));
	assert_int_equal(raw[0], 0x5A);

	/* A wrong size does not write the output. */
	out = 7;
	assert_false(calldata_get_data(&cd, "a", &out, sizeof(out) - 1));
	assert_int_equal(out, 7);

	calldata_free(&cd);
	assert_int_equal(bnum_allocs(), allocs);
}

static void capacity_boundary_test(void **state)
{
	UNUSED_PARAMETER(state);

	struct calldata cd;
	char name[128];
	size_t under = 128 - sizeof(size_t) * 3 - 1;
	size_t exact = 128 - sizeof(size_t) * 3;

	assert_true(under > 1);
	assert_true(exact > 1);

	memset(name, 'n', under - 1);
	name[under - 1] = '\0';
	calldata_init(&cd);
	calldata_set_data(&cd, name, NULL, 0);
	assert_int_equal(cd.size, param_bytes(under, 0));
	assert_int_equal(cd.size, 127);
	assert_int_equal(cd.capacity, 128);
	calldata_free(&cd);

	memset(name, 'n', exact - 1);
	name[exact - 1] = '\0';
	calldata_init(&cd);
	calldata_set_data(&cd, name, NULL, 0);
	assert_int_equal(cd.size, 128);
	assert_int_equal(cd.capacity, 128);
	calldata_free(&cd);
}

static void string_test(void **state)
{
	UNUSED_PARAMETER(state);

	struct calldata cd;
	const char *str = (const char *)(uintptr_t)1;
	const char *hello = "hello";
	size_t hello_len = strlen(hello) + 1;
	size_t offset = 0;

	calldata_init(&cd);
	calldata_set_string(&cd, "hello", hello);
	assert_true(calldata_get_string(&cd, "hello", &str));
	assert_non_null(str);
	assert_string_equal(str, hello);
	assert_ptr_equal(str, (const char *)(cd.stack + sizeof(size_t) + hello_len + sizeof(size_t)));
	expect_param(&cd, &offset, "hello", hello, hello_len);
	expect_end(&cd, offset);

	/* Characterized, not endorsed: a null string is stored as a zero-size
	 * parameter and get_string reports success with a null pointer. */
	calldata_set_string(&cd, "none", NULL);
	str = (const char *)(uintptr_t)1;
	assert_true(calldata_get_string(&cd, "none", &str));
	assert_null(str);
	assert_null(calldata_string(&cd, "none"));

	calldata_set_string(&cd, "blank", "");
	assert_true(calldata_get_string(&cd, "blank", &str));
	assert_non_null(str);
	assert_string_equal(str, "");

	str = (const char *)(uintptr_t)1;
	assert_false(calldata_get_string(&cd, "missing", &str));
	assert_ptr_equal(str, (const char *)(uintptr_t)1);
	assert_null(calldata_string(&cd, "missing"));

	calldata_free(&cd);
}

static void resize_keeps_later_param_test(void **state)
{
	UNUSED_PARAMETER(state);

	struct calldata cd;
	long long first = 11;
	long long second = 22;
	long long out = 0;
	uint8_t *stack;
	size_t capacity;
	size_t size;
	uint8_t grown[32];
	size_t offset;

	calldata_init(&cd);
	calldata_set_int(&cd, "a", first);
	calldata_set_int(&cd, "b", second);
	stack = cd.stack;
	capacity = cd.capacity;
	size = cd.size;

	/* Same size: no move, no new allocation. */
	calldata_set_int(&cd, "a", 99);
	assert_ptr_equal(cd.stack, stack);
	assert_int_equal(cd.capacity, capacity);
	assert_int_equal(cd.size, size);
	assert_int_equal(calldata_int(&cd, "a"), 99);
	assert_int_equal(calldata_int(&cd, "b"), second);

	memset(grown, 0x3C, sizeof(grown));
	calldata_set_data(&cd, "a", grown, sizeof(grown));
	assert_int_equal(cd.size, size + (sizeof(grown) - sizeof(first)));
	assert_true(calldata_get_data(&cd, "a", grown, sizeof(grown)));
	assert_int_equal(calldata_int(&cd, "b"), second);
	offset = 0;
	expect_param(&cd, &offset, "a", grown, sizeof(grown));
	expect_param(&cd, &offset, "b", &second, sizeof(second));
	expect_end(&cd, offset);

	first = 33;
	calldata_set_int(&cd, "a", first);
	assert_int_equal(calldata_int(&cd, "a"), first);
	assert_int_equal(calldata_int(&cd, "b"), second);
	offset = 0;
	expect_param(&cd, &offset, "a", &first, sizeof(first));
	expect_param(&cd, &offset, "b", &second, sizeof(second));
	expect_end(&cd, offset);

	/* Shrinking to an empty payload keeps the name and the later one. */
	calldata_set_data(&cd, "a", NULL, 0);
	assert_false(calldata_get_int(&cd, "a", &out));
	assert_true(calldata_get_data(&cd, "a", &out, 0));
	assert_int_equal(calldata_int(&cd, "b"), second);

	calldata_free(&cd);
}

static void exact_capacity_fits_test(void **state)
{
	UNUSED_PARAMETER(state);

	struct calldata cd;
	uint8_t blob[128];
	size_t size;
	size_t extra;
	size_t overhead;
	size_t data_size;

	calldata_init(&cd);
	calldata_set_data(&cd, "a", "Z", 1);
	size = cd.size;
	assert_int_equal(cd.capacity, 128);
	assert_true(size < 128);

	/* An exact fit is a fit: the needed size already includes the
	 * terminating size field. This supersedes the earlier
	 * characterization (grow when the needed size equals capacity), which
	 * wasted a reallocation and made fixed buffers refuse exact fits. */
	extra = 128 - size;
	overhead = 2 + sizeof(size_t) * 2;
	assert_true(extra > overhead);
	data_size = extra - overhead;
	memset(blob, 0x11, data_size);
	calldata_set_data(&cd, "b", blob, data_size);

	assert_int_equal(cd.size, 128);
	assert_int_equal(cd.capacity, 128);
	{
		uint8_t got[128];
		assert_true(data_size <= sizeof(got));
		assert_true(calldata_get_data(&cd, "a", got, 1));
		assert_int_equal(got[0], 'Z');
		assert_true(calldata_get_data(&cd, "b", got, data_size));
		assert_memory_equal(got, blob, data_size);
	}

	calldata_free(&cd);
}

static void growth_past_double_test(void **state)
{
	UNUSED_PARAMETER(state);

	struct calldata cd;
	uint8_t blob[512];
	size_t target = 300;
	size_t overhead;
	size_t data_size;
	long long out = 0;

	calldata_init(&cd);
	calldata_set_int(&cd, "a", 5);
	assert_int_equal(cd.capacity, 128);

	overhead = 2 + sizeof(size_t) * 2;
	assert_true(target > cd.size + overhead);
	assert_true(target > cd.capacity * 2);
	data_size = target - cd.size - overhead;
	assert_true(data_size <= sizeof(blob));
	memset(blob, 0x22, data_size);
	calldata_set_data(&cd, "b", blob, data_size);

	assert_int_equal(cd.size, target);
	assert_int_equal(cd.capacity, target);
	assert_true(calldata_get_int(&cd, "a", &out));
	assert_int_equal(out, 5);

	calldata_free(&cd);
}

static void typed_helpers_test(void **state)
{
	UNUSED_PARAMETER(state);

	struct calldata cd;
	long long n = 0;
	double f = 1.0;
	bool flag = false;
	int obj = 7;
	long long one = 1;

	calldata_init(&cd);
	calldata_set_int(&cd, "n", 1);
	calldata_set_float(&cd, "f", 1.5);
	calldata_set_bool(&cd, "flag", true);
	calldata_set_ptr(&cd, "p", &obj);

	assert_true(calldata_get_int(&cd, "n", &n));
	assert_int_equal(n, 1);
	assert_true(calldata_get_float(&cd, "f", &f));
	assert_true(f == 1.5);
	assert_true(calldata_get_bool(&cd, "flag", &flag));
	assert_true(flag);
	assert_ptr_equal(calldata_ptr(&cd, "p"), &obj);

	assert_int_equal(calldata_int(&cd, "missing"), 0);
	assert_true(calldata_float(&cd, "missing") == 0.0);
	assert_false(calldata_bool(&cd, "missing"));
	assert_null(calldata_ptr(&cd, "missing"));

	/* A bool is one byte, so an int get does not accept it. */
	assert_int_equal(sizeof(bool), 1);
	assert_false(calldata_get_int(&cd, "flag", &n));

	/* Characterized, not endorsed: the header says get checks the type.
	 * The stack stores only a size, and long long and double are both
	 * 8 bytes, so get_float succeeds on an int. */
	assert_int_equal(sizeof(long long), sizeof(double));
	f = 0.0;
	assert_true(calldata_get_float(&cd, "n", &f));
	assert_memory_equal(&f, &one, sizeof(f));

	calldata_free(&cd);
}

static void clear_and_reuse_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs;
	struct calldata cd;
	uint8_t *stack;
	size_t capacity;
	uint8_t saved;
	long long out = 0;

	calldata_init(&cd);
	calldata_clear(&cd);
	assert_null(cd.stack);
	assert_int_equal(cd.size, 0);

	calldata_set_int(&cd, "a", 11);
	calldata_set_string(&cd, "b", "hi");
	stack = cd.stack;
	capacity = cd.capacity;
	saved = cd.stack[sizeof(size_t)];
	allocs = bnum_allocs();

	calldata_clear(&cd);
	assert_ptr_equal(cd.stack, stack);
	assert_int_equal(cd.capacity, capacity);
	assert_int_equal(cd.size, sizeof(size_t));
	assert_int_equal(read_size(cd.stack), 0);
	/* Characterized, not endorsed: clear zeroes the first size_t only. */
	assert_int_equal(cd.stack[sizeof(size_t)], saved);
	assert_false(calldata_get_int(&cd, "a", &out));
	assert_null(calldata_string(&cd, "b"));
	assert_int_equal(bnum_allocs(), allocs);

	calldata_set_int(&cd, "c", 9);
	assert_int_equal(bnum_allocs(), allocs);
	assert_int_equal(calldata_int(&cd, "c"), 9);
	assert_false(calldata_get_int(&cd, "a", &out));

	calldata_free(&cd);
}

static void fixed_stack_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	struct log_record rec;
	log_handler_t prev = NULL;
	void *prev_param = NULL;
	uint8_t buf[256];
	uint8_t exact[256];
	uint8_t room[128];
	struct calldata cd;
	struct calldata cd_exact;
	struct calldata cd_room;
	size_t needed = param_bytes(2, sizeof(long long));
	long long out = 0;
	uint8_t bigger[sizeof(long long) + 2];

	memset(&rec, 0, sizeof(rec));
	base_get_log_handler(&prev, &prev_param);
	base_set_log_handler(capture_log, &rec);

	memset(buf, 0xAB, sizeof(buf));
	calldata_init_fixed(&cd, buf, needed + 1);
	assert_true(cd.fixed);
	assert_ptr_equal(cd.stack, buf);
	assert_int_equal(cd.capacity, needed + 1);
	assert_int_equal(cd.size, sizeof(size_t));
	assert_int_equal(read_size(buf), 0);

	calldata_set_int(&cd, "a", 42);
	assert_int_equal(rec.calls, 0);
	assert_int_equal(cd.size, needed);
	assert_true(calldata_get_int(&cd, "a", &out));
	assert_int_equal(out, 42);

	/* A write one byte beyond the fixed capacity is refused and the
	 * previous value stays. (The old characterization, that even the one
	 * spare byte was unusable, is superseded: the capacity check now
	 * accepts an exact fit.) */
	memset(bigger, 7, sizeof(bigger));
	calldata_set_data(&cd, "a", bigger, sizeof(bigger));
	assert_int_equal(rec.calls, 1);
	assert_int_equal(rec.level, LOG_ERROR);
	assert_string_equal(rec.message, "Tried to go above fixed calldata stack size!");
	assert_int_equal(cd.size, needed);
	assert_int_equal(calldata_int(&cd, "a"), 42);

	/* A fixed buffer whose capacity equals the write accepts it: needed
	 * includes the terminator. This supersedes the old characterization
	 * that an exact fit was refused and nothing stored. */
	calldata_init_fixed(&cd_exact, exact, needed);
	calldata_set_int(&cd_exact, "a", 5);
	assert_int_equal(rec.calls, 1);
	assert_int_equal(cd_exact.size, needed);
	assert_true(calldata_get_int(&cd_exact, "a", &out));
	assert_int_equal(out, 5);
	{
		size_t offset = 0;
		long long a = 5;
		expect_param(&cd_exact, &offset, "a", &a, sizeof(a));
		expect_end(&cd_exact, offset);
	}

	calldata_init_fixed(&cd_room, room, sizeof(room));
	calldata_set_int(&cd_room, "a", 1);
	calldata_set_int(&cd_room, "b", 2);
	assert_int_equal(rec.calls, 1);
	assert_int_equal(calldata_int(&cd_room, "a"), 1);
	assert_int_equal(calldata_int(&cd_room, "b"), 2);
	{
		size_t offset = 0;
		long long a = 1;
		long long b = 2;
		expect_param(&cd_room, &offset, "a", &a, sizeof(a));
		expect_param(&cd_room, &offset, "b", &b, sizeof(b));
		expect_end(&cd_room, offset);
	}

	/* free of a fixed stack does not release or clear the buffer. */
	calldata_free(&cd);
	calldata_free(&cd_exact);
	calldata_free(&cd_room);
	assert_ptr_equal(cd_room.stack, room);
	assert_int_equal(calldata_int(&cd_room, "b"), 2);
	assert_int_equal(bnum_allocs(), allocs);

	base_set_log_handler(prev, prev_param);
}

static void create_destroy_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	struct calldata *cd = calldata_create();

	assert_non_null(cd);
	assert_int_equal(bnum_allocs(), allocs + 1);
	calldata_set_int(cd, "a", 4);
	assert_int_equal(bnum_allocs(), allocs + 2);
	assert_int_equal(calldata_int(cd, "a"), 4);

	calldata_destroy(cd);
	assert_int_equal(bnum_allocs(), allocs);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(guards_test),
		cmocka_unit_test(first_param_test),
		cmocka_unit_test(capacity_boundary_test),
		cmocka_unit_test(string_test),
		cmocka_unit_test(resize_keeps_later_param_test),
		cmocka_unit_test(exact_capacity_fits_test),
		cmocka_unit_test(growth_past_double_test),
		cmocka_unit_test(typed_helpers_test),
		cmocka_unit_test(clear_and_reuse_test),
		cmocka_unit_test(fixed_stack_test),
		cmocka_unit_test(create_destroy_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
