#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include <obs.h>
#include <util/base.h>
#include <util/bmem.h>

/* Characterization of libobs/obs-missing-files.c.h before its port (#1459).
 * The expected values are what that file does today. Cases marked
 * "characterized, not endorsed" pin odd behavior of that C.
 *
 * Not called, because the C crashes or does not return:
 * - destroy, count, add, get, or append on a null list
 * - get_path, get_source_name, destroy, or issue_callback on a null file
 * - get_file with an index outside the list
 * - append of a list to itself (the loop grows the same array it walks)
 * - issue_callback of a SOURCE file whose callback is null and whose source
 *   is live (obs_source_replace_missing_file calls the null pointer)
 * - adding the same file twice without an extra reference (destroy releases
 *   each slot, so the second release uses a freed file)
 */

static int calls;
static void *got_src;
static const char *got_path;
static void *got_data;
static int private_data;
static bool obs_up;

static void on_replace(void *src, const char *new_path, void *data)
{
	calls++;
	got_src = src;
	got_path = new_path;
	got_data = data;
}

static void log_handler(int level, const char *format, va_list args, void *param)
{
	(void)param;
	if (level > LOG_INFO)
		return;
	vfprintf(stderr, format, args);
	fputc('\n', stderr);
}

static void *source_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	(void)source;
	return &private_data;
}

static void source_destroy(void *data)
{
	(void)data;
}

static const char *source_name(void *type_data)
{
	(void)type_data;
	return "missing files";
}

static uint32_t source_size(void *data)
{
	(void)data;
	return 64;
}

static void register_source(void)
{
	struct obs_source_info info = {
		.id = "missing_files_char_input",
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = source_name,
		.create = source_create,
		.destroy = source_destroy,
		.get_width = source_size,
		.get_height = source_size,
	};

	obs_register_source(&info);
}

static int source_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (obs_up)
		register_source();
	return 0;
}

static void no_task(void *param)
{
	(void)param;
}

static int source_teardown(void **state)
{
	(void)state;
	if (obs_up) {
		obs_queue_task(OBS_TASK_DESTROY, no_task, NULL, true);
		obs_shutdown();
		obs_up = false;
	}
	return 0;
}

static void empty_list_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_files_t *files = obs_missing_files_create();

	assert_non_null(files);
	assert_int_equal(obs_missing_files_count(files), 0);
	assert_int_equal(bnum_allocs(), before + 1);

	obs_missing_files_destroy(files);
	assert_int_equal(bnum_allocs(), before);
}

static void path_copy_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	char path[] = "shot.png";
	obs_missing_file_t *file = obs_missing_file_create(path, NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);

	path[0] = 'x';
	assert_string_equal(obs_missing_file_get_path(file), "shot.png");
	assert_ptr_not_equal(obs_missing_file_get_path(file), path);
	assert_null(obs_missing_file_get_source_name(file));
	/* struct + path copy. A script stores no source name. */
	assert_int_equal(bnum_allocs(), before + 2);

	obs_missing_file_release(file);
	assert_int_equal(bnum_allocs(), before);
}

static void null_and_empty_path_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_file_t *null_path = obs_missing_file_create(NULL, NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);
	obs_missing_file_t *empty_path = obs_missing_file_create("", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);

	assert_null(obs_missing_file_get_path(null_path));
	assert_string_equal(obs_missing_file_get_path(empty_path), "");
	assert_ptr_not_equal(obs_missing_file_get_path(empty_path), "");
	/* null path is not allocated. The empty path is one byte plus the NUL. */
	assert_int_equal(bnum_allocs(), before + 3);

	obs_missing_file_release(null_path);
	obs_missing_file_release(empty_path);
	assert_int_equal(bnum_allocs(), before);
}

/* characterized, not endorsed: destroy frees the file while its ref is still 1. */
static void direct_destroy_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_file_t *file = obs_missing_file_create("a.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);

	obs_missing_file_destroy(file);
	assert_int_equal(bnum_allocs(), before);
}

static void release_null_test(void **state)
{
	(void)state;

	long before = bnum_allocs();

	obs_missing_file_release(NULL);
	assert_int_equal(bnum_allocs(), before);
}

/* characterized, not endorsed: add_file does not take a reference. Destroying
 * the list is what frees the file. */
static void add_does_not_ref_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_files_t *files = obs_missing_files_create();
	obs_missing_file_t *file = obs_missing_file_create("a.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);

	obs_missing_files_add_file(files, file);
	assert_int_equal(obs_missing_files_count(files), 1);
	assert_ptr_equal(obs_missing_files_get_file(files, 0), file);
	assert_string_equal(obs_missing_file_get_path(file), "a.png");

	obs_missing_files_destroy(files);
	assert_int_equal(bnum_allocs(), before);
}

static void insertion_order_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_files_t *files = obs_missing_files_create();
	obs_missing_file_t *first = obs_missing_file_create("first.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);
	obs_missing_file_t *second = obs_missing_file_create("second.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);
	long after_add;

	obs_missing_files_add_file(files, first);
	obs_missing_files_add_file(files, second);
	assert_int_equal(obs_missing_files_count(files), 2);

	after_add = bnum_allocs();
	assert_ptr_equal(obs_missing_files_get_file(files, 0), first);
	assert_ptr_equal(obs_missing_files_get_file(files, 1), second);
	assert_int_equal(bnum_allocs(), after_add);

	obs_missing_files_destroy(files);
	assert_int_equal(bnum_allocs(), before);
}

/* characterized, not endorsed: a null entry is stored, and destroy releases it
 * with obs_missing_file_release(NULL), which returns. */
static void null_entry_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_files_t *files = obs_missing_files_create();
	long after_get;

	obs_missing_files_add_file(files, NULL);
	assert_int_equal(obs_missing_files_count(files), 1);
	after_get = bnum_allocs();
	assert_null(obs_missing_files_get_file(files, 0));
	assert_int_equal(bnum_allocs(), after_get);

	obs_missing_files_destroy(files);
	assert_int_equal(bnum_allocs(), before);
}

static void append_keeps_files_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_files_t *src = obs_missing_files_create();
	obs_missing_files_t *dst = obs_missing_files_create();
	obs_missing_file_t *a = obs_missing_file_create("a.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);
	obs_missing_file_t *b = obs_missing_file_create("b.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);
	long after_src_gone;

	obs_missing_files_add_file(src, a);
	obs_missing_files_add_file(src, b);
	obs_missing_files_append(dst, src);
	assert_int_equal(obs_missing_files_count(dst), 2);
	assert_ptr_equal(obs_missing_files_get_file(dst, 0), a);
	assert_ptr_equal(obs_missing_files_get_file(dst, 1), b);

	/* src's release drops one ref each. append's ref keeps the files. */
	obs_missing_files_destroy(src);
	after_src_gone = bnum_allocs();
	assert_string_equal(obs_missing_file_get_path(obs_missing_files_get_file(dst, 0)), "a.png");
	assert_string_equal(obs_missing_file_get_path(obs_missing_files_get_file(dst, 1)), "b.png");
	assert_int_equal(bnum_allocs(), after_src_gone);

	obs_missing_files_destroy(dst);
	assert_int_equal(bnum_allocs(), before);
}

static void append_empty_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	obs_missing_files_t *src = obs_missing_files_create();
	obs_missing_files_t *dst = obs_missing_files_create();
	obs_missing_file_t *file = obs_missing_file_create("a.png", NULL, OBS_MISSING_FILE_SCRIPT, NULL, NULL);

	obs_missing_files_add_file(dst, file);
	obs_missing_files_append(dst, src);
	assert_int_equal(obs_missing_files_count(dst), 1);
	assert_ptr_equal(obs_missing_files_get_file(dst, 0), file);

	obs_missing_files_destroy(src);
	obs_missing_files_destroy(dst);
	assert_int_equal(bnum_allocs(), before);
}

/* characterized, not endorsed: a script file stores the callback and never
 * calls it. An unknown type does the same, and stores no source name. */
static void script_and_unknown_issue_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	int marker = 0;
	obs_missing_file_t *script = obs_missing_file_create("script.lua", on_replace, OBS_MISSING_FILE_SCRIPT,
							     (void *)(uintptr_t)1, &marker);
	obs_missing_file_t *other = obs_missing_file_create("other.bin", on_replace, 2, (void *)(uintptr_t)2, &marker);

	calls = 0;
	obs_missing_file_issue_callback(script, "new.lua");
	obs_missing_file_issue_callback(other, NULL);
	assert_int_equal(calls, 0);
	assert_null(obs_missing_file_get_source_name(script));
	assert_null(obs_missing_file_get_source_name(other));
	assert_string_equal(obs_missing_file_get_path(script), "script.lua");
	assert_string_equal(obs_missing_file_get_path(other), "other.bin");

	obs_missing_file_release(script);
	obs_missing_file_release(other);
	assert_int_equal(bnum_allocs(), before);
}

/* characterized, not endorsed: a SOURCE file with a null source stores a null
 * name, and issue_callback returns without calling. */
static void null_source_issue_test(void **state)
{
	(void)state;

	long before = bnum_allocs();
	int marker = 0;
	obs_missing_file_t *file = obs_missing_file_create("a.png", on_replace, OBS_MISSING_FILE_SOURCE, NULL, &marker);

	calls = 0;
	assert_null(obs_missing_file_get_source_name(file));
	obs_missing_file_issue_callback(file, "b.png");
	assert_int_equal(calls, 0);
	assert_int_equal(bnum_allocs(), before + 2);

	obs_missing_file_release(file);
	assert_int_equal(bnum_allocs(), before);
}

static void source_snapshot_test(void **state)
{
	obs_source_t *source;
	obs_missing_file_t *file;
	int marker = 0;
	long before_file;
	long after_file;
	long after_ops;
	char new_path[] = "fixed.png";

	(void)state;
	if (!obs_up)
		skip();

	source = obs_source_create("missing_files_char_input", "clip", NULL, NULL);
	assert_non_null(source);

	before_file = bnum_allocs();
	file = obs_missing_file_create("clip.png", on_replace, OBS_MISSING_FILE_SOURCE, source, &marker);
	after_file = bnum_allocs();
	/* struct, path, and a copy of the source name. */
	assert_int_equal(after_file, before_file + 3);
	assert_string_equal(obs_missing_file_get_path(file), "clip.png");
	assert_string_equal(obs_missing_file_get_source_name(file), "clip");
	assert_ptr_not_equal(obs_missing_file_get_source_name(file), obs_source_get_name(source));

	obs_source_set_name(source, "renamed");
	assert_string_equal(obs_source_get_name(source), "renamed");
	assert_string_equal(obs_missing_file_get_source_name(file), "clip");

	calls = 0;
	obs_missing_file_issue_callback(file, new_path);
	assert_int_equal(calls, 1);
	assert_ptr_equal(got_src, &private_data);
	assert_ptr_equal(got_path, new_path);
	assert_ptr_equal(got_data, &marker);

	calls = 0;
	obs_missing_file_issue_callback(file, NULL);
	assert_int_equal(calls, 1);
	assert_null(got_path);
	assert_ptr_equal(got_src, &private_data);
	assert_ptr_equal(got_data, &marker);

	after_ops = bnum_allocs();
	obs_missing_file_release(file);
	assert_int_equal(bnum_allocs(), after_ops - 3);

	obs_source_remove(source);
	obs_source_release(source);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(empty_list_test),
		cmocka_unit_test(path_copy_test),
		cmocka_unit_test(null_and_empty_path_test),
		cmocka_unit_test(direct_destroy_test),
		cmocka_unit_test(release_null_test),
		cmocka_unit_test(add_does_not_ref_test),
		cmocka_unit_test(insertion_order_test),
		cmocka_unit_test(null_entry_test),
		cmocka_unit_test(append_keeps_files_test),
		cmocka_unit_test(append_empty_test),
		cmocka_unit_test(script_and_unknown_issue_test),
		cmocka_unit_test(null_source_issue_test),
		cmocka_unit_test_setup_teardown(source_snapshot_test, source_setup, source_teardown),
	};

	base_set_log_handler(log_handler, NULL);
	return cmocka_run_group_tests(tests, NULL, NULL);
}
