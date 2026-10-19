#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <callback/calldata.h>
#include <callback/proc.h>
#include <util/base.h>
#include <util/bmem.h>

/* Characterization tests: the expected values are what
 * libobs/callback/proc.c.h produces today (#1440). Cases marked
 * "characterized, not endorsed" pin odd behavior of that C. */

struct log_record {
	int calls;
	int level;
	char message[160];
};

struct call_seen {
	int hits;
	void *data;
	calldata_t *params;
	long long n;
	proc_handler_t *handler;
};

static void capture_log(int level, const char *format, va_list args, void *param)
{
	struct log_record *rec = param;

	rec->calls++;
	rec->level = level;
	vsnprintf(rec->message, sizeof(rec->message), format, args);
}

static void count_call(void *data, calldata_t *params)
{
	struct call_seen *seen = data;

	seen->hits++;
	seen->data = data;
	seen->params = params;
	if (params)
		seen->n = calldata_int(params, "n");
}

static void mark_second(void *data, calldata_t *params)
{
	int *which = data;

	UNUSED_PARAMETER(params);
	*which = 2;
}

static void inner_call(void *data, calldata_t *params)
{
	struct call_seen *seen = data;

	UNUSED_PARAMETER(params);
	seen->hits += 10;
}

static void outer_call(void *data, calldata_t *params)
{
	struct call_seen *seen = data;

	seen->hits += 1;
	proc_handler_add(seen->handler, "void inner()", inner_call, seen);
	assert_true(proc_handler_call(seen->handler, "inner", params));
	/* A second add of the same name, from inside the running proc,
	 * does not replace the first callback. */
	proc_handler_add(seen->handler, "void inner()", count_call, seen);
	assert_true(proc_handler_call(seen->handler, "inner", params));
}

static void destroy_from_call(void *data, calldata_t *params)
{
	proc_handler_t **slot = data;

	UNUSED_PARAMETER(params);
	proc_handler_destroy(*slot);
	*slot = NULL;
}

static void null_handler_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	struct call_seen seen;
	struct calldata cd;

	memset(&seen, 0, sizeof(seen));
	calldata_init(&cd);

	proc_handler_destroy(NULL);
	proc_handler_add(NULL, "void f()", count_call, &seen);
	assert_false(proc_handler_call(NULL, "f", &cd));
	assert_int_equal(seen.hits, 0);
	assert_int_equal(bnum_allocs(), allocs);

	calldata_free(&cd);
}

static void create_destroy_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();

	assert_non_null(handler);
	proc_handler_destroy(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

static void call_passes_params_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();
	struct call_seen seen;
	struct calldata cd;
	struct log_record rec;
	log_handler_t prev = NULL;
	void *prev_param = NULL;

	memset(&seen, 0, sizeof(seen));
	memset(&rec, 0, sizeof(rec));
	calldata_init(&cd);
	calldata_set_int(&cd, "n", 7);

	base_get_log_handler(&prev, &prev_param);
	base_set_log_handler(capture_log, &rec);

	proc_handler_add(handler, "void f(in int n)", count_call, &seen);
	assert_int_equal(rec.calls, 0);
	assert_true(proc_handler_call(handler, "f", &cd));
	assert_int_equal(seen.hits, 1);
	assert_ptr_equal(seen.data, &seen);
	assert_ptr_equal(seen.params, &cd);
	assert_int_equal(seen.n, 7);

	/* The declaration is stored for its name. The call does not check
	 * the calldata against that declaration. */
	memset(&seen, 0, sizeof(seen));
	assert_true(proc_handler_call(handler, "f", NULL));
	assert_int_equal(seen.hits, 1);
	assert_null(seen.params);
	assert_int_equal(rec.calls, 0);

	/* "void  spaced()" parses. Lookup is the name, not the decl text. */
	proc_handler_add(handler, "void  spaced()", count_call, &seen);
	assert_true(proc_handler_call(handler, "spaced", &cd));
	assert_false(proc_handler_call(handler, "void  spaced()", NULL));
	assert_int_equal(seen.hits, 2);

	base_set_log_handler(prev, prev_param);
	proc_handler_destroy(handler);
	calldata_free(&cd);
	assert_int_equal(bnum_allocs(), allocs);
}

static void unknown_name_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();
	struct call_seen foo;
	struct call_seen bar;
	struct log_record rec;
	log_handler_t prev = NULL;
	void *prev_param = NULL;

	memset(&foo, 0, sizeof(foo));
	memset(&bar, 0, sizeof(bar));
	memset(&rec, 0, sizeof(rec));
	base_get_log_handler(&prev, &prev_param);
	base_set_log_handler(capture_log, &rec);

	/* No procs yet, so the NULL name never reaches strcmp. */
	assert_false(proc_handler_call(handler, NULL, NULL));
	assert_false(proc_handler_call(handler, "", NULL));
	assert_false(proc_handler_call(handler, "missing", NULL));
	assert_int_equal(rec.calls, 0);

	proc_handler_add(handler, "void foo()", count_call, &foo);
	proc_handler_add(handler, "void foobar()", count_call, &bar);
	proc_handler_add(handler, "void Foo()", count_call, &foo);

	assert_true(proc_handler_call(handler, "foo", NULL));
	assert_int_equal(foo.hits, 1);
	assert_int_equal(bar.hits, 0);
	assert_true(proc_handler_call(handler, "foobar", NULL));
	assert_int_equal(bar.hits, 1);
	assert_true(proc_handler_call(handler, "Foo", NULL));
	assert_int_equal(foo.hits, 2);
	assert_false(proc_handler_call(handler, "FOO", NULL));
	assert_false(proc_handler_call(handler, "fo", NULL));
	assert_int_equal(rec.calls, 0);

	/* Characterized, not endorsed: a NULL name once any proc is
	 * registered faults in strcmp. Not called here. */

	base_set_log_handler(prev, prev_param);
	proc_handler_destroy(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

static void invalid_decl_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();
	struct call_seen seen;
	struct log_record rec;
	log_handler_t prev = NULL;
	void *prev_param = NULL;

	memset(&seen, 0, sizeof(seen));
	memset(&rec, 0, sizeof(rec));
	base_get_log_handler(&prev, &prev_param);
	base_set_log_handler(capture_log, &rec);

	proc_handler_add(handler, "void f(foo a)", count_call, &seen);
	assert_true(rec.calls >= 1);
	assert_int_equal(rec.level, LOG_ERROR);
	assert_string_equal(rec.message, "Function declaration invalid: void f(foo a)");
	assert_false(proc_handler_call(handler, "f", NULL));
	assert_int_equal(seen.hits, 0);

	rec.calls = 0;
	proc_handler_add(handler, "", count_call, &seen);
	assert_int_equal(rec.level, LOG_ERROR);
	assert_string_equal(rec.message, "Function declaration invalid: ");
	assert_false(proc_handler_call(handler, "", NULL));

	rec.calls = 0;
	proc_handler_add(handler, NULL, count_call, &seen);
	assert_int_equal(rec.level, LOG_ERROR);
	assert_string_equal(rec.message, "Function declaration invalid: (null)");
	assert_int_equal(seen.hits, 0);

	/* A later valid add still registers. */
	proc_handler_add(handler, "void f()", count_call, &seen);
	assert_true(proc_handler_call(handler, "f", NULL));
	assert_int_equal(seen.hits, 1);

	base_set_log_handler(prev, prev_param);
	proc_handler_destroy(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

static void mark_first(void *data, calldata_t *params)
{
	int *which = data;

	UNUSED_PARAMETER(params);
	*which = 1;
}

static void duplicate_name_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();
	int which = 0;
	struct log_record rec;
	log_handler_t prev = NULL;
	void *prev_param = NULL;

	memset(&rec, 0, sizeof(rec));
	base_get_log_handler(&prev, &prev_param);
	base_set_log_handler(capture_log, &rec);

	proc_handler_add(handler, "void f()", mark_first, &which);
	proc_handler_add(handler, "int f(out string s)", mark_second, &which);
	assert_int_equal(rec.calls, 1);
	assert_int_equal(rec.level, LOG_WARNING);
	assert_string_equal(rec.message, "Procedure 'f' already exists");

	assert_true(proc_handler_call(handler, "f", NULL));
	assert_int_equal(which, 1);

	base_set_log_handler(prev, prev_param);
	proc_handler_destroy(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

static void reenter_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();
	struct call_seen seen;
	struct log_record rec;
	log_handler_t prev = NULL;
	void *prev_param = NULL;

	memset(&seen, 0, sizeof(seen));
	memset(&rec, 0, sizeof(rec));
	seen.handler = handler;
	base_get_log_handler(&prev, &prev_param);
	base_set_log_handler(capture_log, &rec);

	/* The call copies the callback and drops the mutex before it runs,
	 * so the callback can add and call on the same handler. The mutex
	 * is created recursive; this path does not nest the lock. */
	proc_handler_add(handler, "void outer()", outer_call, &seen);
	assert_true(proc_handler_call(handler, "outer", NULL));
	assert_int_equal(seen.hits, 21);
	assert_int_equal(rec.calls, 1);
	assert_int_equal(rec.level, LOG_WARNING);
	assert_string_equal(rec.message, "Procedure 'inner' already exists");

	base_set_log_handler(prev, prev_param);
	proc_handler_destroy(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

static void destroy_from_callback_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();

	proc_handler_add(handler, "void stop()", destroy_from_call, &handler);
	assert_true(proc_handler_call(handler, "stop", NULL));
	assert_null(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

static void null_proc_test(void **state)
{
	UNUSED_PARAMETER(state);

	long allocs = bnum_allocs();
	proc_handler_t *handler = proc_handler_create();

	/* Characterized, not endorsed: a NULL callback is stored. Calling
	 * it faults. Destroy still releases the declaration. */
	proc_handler_add(handler, "void none()", NULL, NULL);
	proc_handler_destroy(handler);
	assert_int_equal(bnum_allocs(), allocs);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(null_handler_test),
		cmocka_unit_test(create_destroy_test),
		cmocka_unit_test(call_passes_params_test),
		cmocka_unit_test(unknown_name_test),
		cmocka_unit_test(invalid_decl_test),
		cmocka_unit_test(duplicate_name_test),
		cmocka_unit_test(reenter_test),
		cmocka_unit_test(destroy_from_callback_test),
		cmocka_unit_test(null_proc_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
