#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <obs-data.h>
#include <graphics/vec2.h>
#include <util/bmem.h>
#include <util/platform.h>

/*
 * Characterization tests for libobs/obs-data.c. They pin CURRENT behavior,
 * including odd behavior. Cases marked "characterized, not endorsed" are
 * quirks of the C implementation that a port must reproduce.
 *
 * User, default, and autoselect values share one item allocation. A getter
 * for the user value reads the bytes at the front of that allocation, so a
 * missing user value returns the default if one is present, and otherwise
 * the autoselect. JSON export still omits an item that has no user value,
 * unless the with-defaults call is used, and that call emits whatever the
 * user getter reads.
 */

#define TMP_DIR "test_obs_data.tmp"

#define SAVE_PATH TMP_DIR "/save.json"
#define SAFE_PATH TMP_DIR "/safe.json"
#define SAFE_TMP TMP_DIR "/safe.json.tmp"
#define SAFE_BAK TMP_DIR "/safe.json.bak"
#define BROKEN_PATH TMP_DIR "/broken.json"
#define BROKEN_BAK TMP_DIR "/broken.json.bak"

static const char *const test_files[] = {
	SAVE_PATH, SAFE_PATH, SAFE_TMP, SAFE_BAK, BROKEN_PATH, BROKEN_BAK,
};

static void remove_test_files(void)
{
	for (size_t i = 0; i < sizeof(test_files) / sizeof(test_files[0]); i++)
		os_unlink(test_files[i]);
}

static int group_setup(void **state)
{
	UNUSED_PARAMETER(state);

	remove_test_files();
	return os_mkdirs(TMP_DIR) == MKDIR_ERROR ? -1 : 0;
}

static int group_teardown(void **state)
{
	UNUSED_PARAMETER(state);

	remove_test_files();
	os_rmdir(TMP_DIR);
	return 0;
}

static void test_null_and_missing(void **state)
{
	obs_data_t *data;

	UNUSED_PARAMETER(state);

	obs_data_release(NULL);
	obs_data_addref(NULL);
	obs_data_set_string(NULL, "a", "b");
	obs_data_set_int(NULL, "a", 1);
	obs_data_erase(NULL, "a");
	obs_data_clear(NULL);
	obs_data_apply(NULL, NULL);
	assert_null(obs_data_get_json(NULL));
	assert_null(obs_data_get_last_json(NULL));
	assert_false(obs_data_save_json(NULL, SAVE_PATH));
	assert_null(obs_data_create_from_json("{"));
	assert_null(obs_data_create_from_json_file(TMP_DIR "/missing.json"));

	data = obs_data_create();
	assert_non_null(data);
	assert_string_equal(obs_data_get_string(data, "missing"), "");
	assert_int_equal(obs_data_get_int(data, "missing"), 0);
	assert_float_equal(obs_data_get_double(data, "missing"), 0.0, 0.0);
	assert_false(obs_data_get_bool(data, "missing"));
	assert_null(obs_data_get_obj(data, "missing"));
	assert_null(obs_data_get_array(data, "missing"));
	assert_false(obs_data_has_user_value(data, "missing"));
	assert_string_equal(obs_data_get_json(data), "{}");
	assert_string_equal(obs_data_get_last_json(data), "{}");

	/* json_loads here has no JSON_DECODE_ANY, so only objects and
	 * arrays parse. An array root becomes an empty obs_data. A
	 * scalar root is rejected. Characterized, not endorsed. */
	obs_data_release(data);
	data = obs_data_create_from_json("[]");
	assert_non_null(data);
	assert_string_equal(obs_data_get_json(data), "{}");
	obs_data_release(data);

	assert_null(obs_data_create_from_json("1"));
	assert_null(obs_data_create_from_json("null"));
	assert_null(obs_data_create_from_json("true"));
	assert_null(obs_data_create_from_json("\"hi\""));
}

static void test_scalars_and_json(void **state)
{
	obs_data_t *data;
	obs_data_t *round;
	const char *pretty;

	UNUSED_PARAMETER(state);

	data = obs_data_create();
	obs_data_set_string(data, "s", NULL);
	obs_data_set_int(data, "i", -4);
	obs_data_set_double(data, "d", 1.5);
	obs_data_set_bool(data, "t", true);
	obs_data_set_bool(data, "f", false);

	assert_string_equal(obs_data_get_string(data, "s"), "");
	assert_int_equal(obs_data_get_int(data, "i"), -4);
	assert_float_equal(obs_data_get_double(data, "d"), 1.5, 0.0);
	assert_true(obs_data_get_bool(data, "t"));
	assert_false(obs_data_get_bool(data, "f"));
	assert_true(obs_data_has_user_value(data, "f"));
	assert_string_equal(obs_data_get_json(data), "{\"s\":\"\",\"i\":-4,\"d\":1.5,\"t\":true,\"f\":false}");

	pretty = obs_data_get_json_pretty(data);
	assert_non_null(strstr(pretty, "\n    \"s\": \"\""));

	/* An int read back from a double truncates toward zero. */
	assert_int_equal(obs_data_get_int(data, "d"), 1);

	round = obs_data_create_from_json(obs_data_get_json(data));
	assert_non_null(round);
	assert_string_equal(obs_data_get_string(round, "s"), "");
	assert_int_equal(obs_data_get_int(round, "i"), -4);
	assert_float_equal(obs_data_get_double(round, "d"), 1.5, 0.0);
	{
		obs_data_item_t *item = obs_data_item_byname(round, "i");

		assert_int_equal(obs_data_item_numtype(item), OBS_DATA_NUM_INT);
		obs_data_item_release(&item);
	}
	obs_data_release(round);
	obs_data_release(data);
}

static void test_json_shapes(void **state)
{
	obs_data_t *data;
	obs_data_t *child;
	obs_data_array_t *array;
	obs_data_item_t *item;

	UNUSED_PARAMETER(state);

	assert_null(obs_data_create_from_json("{\"a\":1,\"a\":2}"));

	data = obs_data_create_from_json("{\"n\":null,\"o\":{\"k\":2},\"a\":[1,{\"k\":\"v\"},\"s\",null]}");
	assert_non_null(data);

	/* JSON null is stored as an object item holding a null pointer.
	 * Characterized, not endorsed. */
	item = obs_data_item_byname(data, "n");
	assert_int_equal(obs_data_item_gettype(item), OBS_DATA_OBJECT);
	assert_true(obs_data_item_has_user_value(item));
	obs_data_item_release(&item);
	assert_null(obs_data_get_obj(data, "n"));
	assert_string_equal(obs_data_get_json(data), "{\"n\":null,\"o\":{\"k\":2},\"a\":[{\"k\":\"v\"}]}");

	child = obs_data_get_obj(data, "o");
	assert_int_equal(obs_data_get_int(child, "k"), 2);
	obs_data_release(child);

	/* Array elements that are not objects are skipped.
	 * Characterized, not endorsed. */
	array = obs_data_get_array(data, "a");
	assert_int_equal(obs_data_array_count(array), 1);
	child = obs_data_array_item(array, 0);
	assert_string_equal(obs_data_get_string(child, "k"), "v");
	assert_null(obs_data_array_item(array, 1));
	obs_data_release(child);
	obs_data_array_release(array);
	obs_data_release(data);
}

static void test_default_and_autoselect(void **state)
{
	obs_data_t *data;
	obs_data_t *defaults;
	obs_data_item_t *item;

	UNUSED_PARAMETER(state);

	data = obs_data_create();
	obs_data_set_default_string(data, "only", "default");
	assert_false(obs_data_has_user_value(data, "only"));
	assert_true(obs_data_has_default_value(data, "only"));
	/* The user getter reads the default bytes. Characterized, not endorsed. */
	assert_string_equal(obs_data_get_string(data, "only"), "default");
	assert_string_equal(obs_data_get_default_string(data, "only"), "default");
	assert_string_equal(obs_data_get_json(data), "{}");
	assert_string_equal(obs_data_get_json_with_defaults(data), "{\"only\":\"default\"}");

	obs_data_set_string(data, "both", "user");
	obs_data_set_default_string(data, "both", "default");
	assert_string_equal(obs_data_get_string(data, "both"), "user");
	assert_string_equal(obs_data_get_default_string(data, "both"), "default");
	assert_string_equal(obs_data_get_json(data), "{\"both\":\"user\"}");
	/* with-defaults still emits the default-only item, and it does
	 * not replace "both" with its default. */
	assert_string_equal(obs_data_get_json_with_defaults(data), "{\"only\":\"default\",\"both\":\"user\"}");

	/* A default whose type does not match the item is ignored. */
	obs_data_set_default_int(data, "both", 9);
	assert_true(obs_data_has_default_value(data, "both"));
	assert_string_equal(obs_data_get_default_string(data, "both"), "default");
	assert_int_equal(obs_data_get_default_int(data, "both"), 0);

	obs_data_set_autoselect_string(data, "auto", "picked");
	assert_false(obs_data_has_user_value(data, "auto"));
	assert_true(obs_data_has_autoselect_value(data, "auto"));
	/* Autoselect-only: the user getter reads those bytes, and the
	 * with-defaults export emits them. Characterized, not endorsed. */
	assert_string_equal(obs_data_get_string(data, "auto"), "picked");
	assert_string_equal(obs_data_get_autoselect_string(data, "auto"), "picked");
	assert_string_equal(obs_data_get_json_with_defaults(data),
			    "{\"only\":\"default\",\"both\":\"user\",\"auto\":\"picked\"}");

	obs_data_set_default_string(data, "layered", "front");
	obs_data_set_autoselect_string(data, "layered", "back");
	assert_string_equal(obs_data_get_string(data, "layered"), "front");
	assert_string_equal(obs_data_get_autoselect_string(data, "layered"), "back");

	obs_data_unset_user_value(data, "both");
	assert_false(obs_data_has_user_value(data, "both"));
	assert_string_equal(obs_data_get_string(data, "both"), "default");
	assert_string_equal(obs_data_get_json(data), "{}");

	/* Setting an autoselect of a different type changes the item type.
	 * Characterized, not endorsed. The test does not export that item:
	 * the user getter would read the old string bytes as a number. */
	obs_data_set_string(data, "retype", "hi");
	obs_data_set_autoselect_int(data, "retype", 7);
	item = obs_data_item_byname(data, "retype");
	assert_int_equal(obs_data_item_gettype(item), OBS_DATA_NUMBER);
	assert_int_equal(obs_data_item_get_autoselect_int(item), 7);
	assert_string_equal(obs_data_item_get_string(item), "");
	obs_data_item_release(&item);

	defaults = obs_data_get_defaults(data);
	assert_string_equal(obs_data_get_string(defaults, "only"), "default");
	assert_true(obs_data_has_user_value(defaults, "only"));
	assert_string_equal(obs_data_get_string(defaults, "both"), "default");
	obs_data_release(defaults);
	obs_data_release(data);

	defaults = obs_data_get_defaults(NULL);
	assert_non_null(defaults);
	assert_string_equal(obs_data_get_json(defaults), "{}");
	obs_data_release(defaults);
}

static void test_apply_erase_clear(void **state)
{
	obs_data_t *src;
	obs_data_t *dst;
	obs_data_t *nested;
	obs_data_t *copied;

	UNUSED_PARAMETER(state);

	src = obs_data_create();
	obs_data_set_string(src, "keep", "a");
	obs_data_set_default_string(src, "skip", "hidden");
	nested = obs_data_create();
	obs_data_set_int(nested, "n", 3);
	obs_data_set_obj(src, "child", nested);
	obs_data_release(nested);

	dst = obs_data_create();
	obs_data_set_string(dst, "keep", "old");
	obs_data_apply(dst, src);
	obs_data_apply(dst, dst);
	assert_string_equal(obs_data_get_string(dst, "keep"), "a");
	assert_false(obs_data_has_user_value(dst, "skip"));
	copied = obs_data_get_obj(dst, "child");
	assert_int_equal(obs_data_get_int(copied, "n"), 3);
	obs_data_set_int(nested, "n", 9);
	assert_int_equal(obs_data_get_int(copied, "n"), 3);
	obs_data_release(copied);

	obs_data_erase(dst, "keep");
	assert_false(obs_data_has_user_value(dst, "keep"));
	assert_string_equal(obs_data_get_string(dst, "keep"), "");
	obs_data_erase(dst, "missing");

	obs_data_set_string(dst, "gone", "x");
	obs_data_set_default_string(dst, "gone", "y");
	obs_data_clear(dst);
	assert_false(obs_data_has_user_value(dst, "gone"));
	assert_true(obs_data_has_default_value(dst, "gone"));
	assert_string_equal(obs_data_get_string(dst, "gone"), "y");

	obs_data_release(src);
	obs_data_release(dst);
}

static void test_arrays_and_iteration(void **state)
{
	obs_data_t *data;
	obs_data_t *first;
	obs_data_t *second;
	obs_data_t *third;
	obs_data_array_t *array;
	obs_data_array_t *extra;
	obs_data_item_t *item;
	obs_data_t *got;

	UNUSED_PARAMETER(state);

	first = obs_data_create();
	second = obs_data_create();
	third = obs_data_create();
	obs_data_set_string(first, "k", "a");
	obs_data_set_string(second, "k", "b");
	obs_data_set_string(third, "k", "c");

	array = obs_data_array_create();
	assert_int_equal(obs_data_array_count(NULL), 0);
	assert_null(obs_data_array_item(NULL, 0));
	assert_int_equal(obs_data_array_push_back(NULL, first), 0);
	assert_int_equal(obs_data_array_push_back(array, NULL), 0);
	obs_data_array_push_back(array, first);
	obs_data_array_push_back(array, second);
	obs_data_array_insert(array, 0, third);
	assert_int_equal(obs_data_array_count(array), 3);
	got = obs_data_array_item(array, 0);
	assert_string_equal(obs_data_get_string(got, "k"), "c");
	obs_data_release(got);
	got = obs_data_array_item(array, 1);
	assert_string_equal(obs_data_get_string(got, "k"), "a");
	obs_data_release(got);

	obs_data_array_erase(array, 1);
	assert_int_equal(obs_data_array_count(array), 2);
	got = obs_data_array_item(array, 1);
	assert_string_equal(obs_data_get_string(got, "k"), "b");
	obs_data_release(got);

	extra = obs_data_array_create();
	obs_data_array_push_back(extra, first);
	obs_data_array_push_back_array(array, extra);
	assert_int_equal(obs_data_array_count(array), 3);
	obs_data_array_release(extra);

	data = obs_data_create();
	obs_data_set_array(data, "arr", array);
	obs_data_set_string(data, "z", "last");
	obs_data_set_int(data, "m", 1);

	item = obs_data_first(data);
	assert_string_equal(obs_data_item_get_name(item), "arr");
	assert_int_equal(obs_data_item_gettype(item), OBS_DATA_ARRAY);
	assert_true(obs_data_item_next(&item));
	assert_string_equal(obs_data_item_get_name(item), "z");
	assert_true(obs_data_item_next(&item));
	assert_string_equal(obs_data_item_get_name(item), "m");
	assert_int_equal(obs_data_item_numtype(item), OBS_DATA_NUM_INT);
	assert_false(obs_data_item_next(&item));
	assert_null(item);

	item = obs_data_item_byname(data, "z");
	obs_data_item_remove(&item);
	/* byname took a reference. remove drops the object's reference
	 * and leaves the caller's, so the pointer stays live. */
	assert_non_null(item);
	assert_string_equal(obs_data_item_get_name(item), "z");
	assert_false(obs_data_has_user_value(data, "z"));
	obs_data_item_release(&item);
	assert_null(item);

	obs_data_array_release(array);
	obs_data_release(first);
	obs_data_release(second);
	obs_data_release(third);
	obs_data_release(data);
}

static void test_vec_and_frames(void **state)
{
	obs_data_t *data;
	struct vec2 in;
	struct vec2 out;
	struct media_frames_per_second fps;
	const char *option;

	UNUSED_PARAMETER(state);

	data = obs_data_create();
	in.x = 1.5f;
	in.y = -2.25f;
	memset(&out, 0, sizeof(out));
	obs_data_set_vec2(data, "v", &in);
	obs_data_get_vec2(data, "v", &out);
	assert_float_equal(out.x, 1.5f, 0.0f);
	assert_float_equal(out.y, -2.25f, 0.0f);
	obs_data_get_vec2(data, "missing", &out);
	assert_float_equal(out.x, 1.5f, 0.0f);

	fps.numerator = 30000;
	fps.denominator = 1001;
	obs_data_set_frames_per_second(data, "rate", fps, NULL);
	memset(&fps, 0, sizeof(fps));
	option = NULL;
	assert_true(obs_data_get_frames_per_second(data, "rate", &fps, &option));
	assert_int_equal(fps.numerator, 30000);
	assert_int_equal(fps.denominator, 1001);
	assert_null(option);

	fps.numerator = 0;
	fps.denominator = 0;
	obs_data_set_frames_per_second(data, "named", fps, "ntsc");
	option = NULL;
	/* The returned option string points into an object this call
	 * releases. Characterized, not endorsed. The test does not read it. */
	assert_true(obs_data_get_frames_per_second(data, "named", &fps, &option));
	assert_false(obs_data_get_frames_per_second(data, "named", &fps, NULL));

	{
		obs_data_t *obj = obs_data_create();
		struct media_frames_per_second bad = {9, 9};

		obs_data_set_int(obj, "numerator", -5);
		obs_data_set_int(obj, "denominator", 1);
		obs_data_set_obj(data, "bad", obj);
		obs_data_release(obj);
		option = "unset";
		assert_false(obs_data_get_frames_per_second(data, "bad", &bad, &option));
		assert_int_equal(bad.numerator, 0);
		assert_int_equal(bad.denominator, 1);
		assert_string_equal(option, "unset");
	}

	obs_data_release(data);
}

static char *read_file(const char *path)
{
	char *text = os_quick_read_utf8_file(path);

	assert_non_null(text);
	return text;
}

static void test_json_files(void **state)
{
	obs_data_t *data;
	obs_data_t *loaded;
	char *text;

	UNUSED_PARAMETER(state);

	data = obs_data_create();
	obs_data_set_string(data, "name", "one");
	assert_true(obs_data_save_json(data, SAVE_PATH));
	loaded = obs_data_create_from_json_file(SAVE_PATH);
	assert_string_equal(obs_data_get_string(loaded, "name"), "one");
	obs_data_release(loaded);

	obs_data_set_string(data, "name", "two");
	assert_true(obs_data_save_json_safe(data, SAFE_PATH, "tmp", "bak"));
	text = read_file(SAFE_PATH);
	assert_string_equal(text, "{\"name\":\"two\"}");
	bfree(text);
	assert_false(os_file_exists(SAFE_TMP));

	obs_data_set_string(data, "name", "three");
	assert_true(obs_data_save_json_safe(data, SAFE_PATH, "tmp", "bak"));
	text = read_file(SAFE_PATH);
	assert_string_equal(text, "{\"name\":\"three\"}");
	bfree(text);
	text = read_file(SAFE_BAK);
	assert_string_equal(text, "{\"name\":\"two\"}");
	bfree(text);

	assert_true(os_quick_write_utf8_file(BROKEN_PATH, "not-json", strlen("not-json"), false));
	assert_true(os_quick_write_utf8_file(BROKEN_BAK, "{\"name\":\"bak\"}", strlen("{\"name\":\"bak\"}"), false));
	assert_null(obs_data_create_from_json_file(BROKEN_PATH));
	loaded = obs_data_create_from_json_file_safe(BROKEN_PATH, "bak");
	assert_non_null(loaded);
	assert_string_equal(obs_data_get_string(loaded, "name"), "bak");
	text = read_file(BROKEN_PATH);
	assert_string_equal(text, "{\"name\":\"bak\"}");
	bfree(text);
	assert_false(os_file_exists(BROKEN_BAK));
	obs_data_release(loaded);

	obs_data_release(data);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_null_and_missing),  cmocka_unit_test(test_scalars_and_json),
		cmocka_unit_test(test_json_shapes),       cmocka_unit_test(test_default_and_autoselect),
		cmocka_unit_test(test_apply_erase_clear), cmocka_unit_test(test_arrays_and_iteration),
		cmocka_unit_test(test_vec_and_frames),    cmocka_unit_test(test_json_files),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
