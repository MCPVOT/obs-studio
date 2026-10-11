/* Regression test for obsproject/obs-studio#9298 (reported by Durss,
 * confirmed by RytoEX and PatTheMav): moving one child of a group slowly
 * drags the group's other children away when the group's alignment is
 * centred on an axis, and PatTheMav saw a bottom-right anchored group creep
 * away from its corner.
 *
 * Moving a child changes the group's content bounds, so the group is resized
 * on the next video tick (resize_group() in obs-scene.c): the children are
 * shifted so the content starts at the group's origin, and the group's
 * position is recomputed so that the content stays where it was. That
 * position is computed from the content's exact size, but the group draws
 * itself with its alignment offset taken from the rounded size: the group
 * is ceil()ed to whole pixels, scaled and truncated, and centred alignment
 * halves it with integer division. Each resize therefore moves the content
 * by the rounding remainder, and the moves add up.
 *
 * The test moves one child of a group in fractional steps, the way dragging
 * in a scaled preview does, waits for the group to resize after each step,
 * and checks where the other child ends up on the canvas. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <math.h>
#include <stdio.h>

#include <obs.h>
#include <graphics/matrix4.h>
#include <util/platform.h>

#define BOX_ID "group_child_drift_box"

static bool obs_up;

static const char *box_name(void *type_data)
{
	(void)type_data;
	return "box";
}

static void *box_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void box_destroy(void *data)
{
	(void)data;
}

static uint32_t box_width(void *data)
{
	(void)data;
	return 100;
}

static uint32_t box_height(void *data)
{
	(void)data;
	return 50;
}

/* Canvas position of a child's top-left corner. */
static struct vec3 child_on_canvas(obs_sceneitem_t *group, obs_sceneitem_t *child)
{
	struct matrix4 box, draw;
	struct vec3 v;

	obs_sceneitem_get_box_transform(child, &box);
	obs_sceneitem_get_draw_transform(group, &draw);
	vec3_set(&v, 0.0f, 0.0f, 0.0f);
	vec3_transform(&v, &v, &box);
	vec3_transform(&v, &v, &draw);
	return v;
}

/* Waits until the video thread has ticked the scene (and resized the group)
 * at least twice. */
static void wait_for_ticks(void)
{
	os_sleep_ms(40);
}

static void check_sibling_stays(uint32_t group_align, const char *what)
{
	obs_scene_t *scene = obs_scene_create("drift");
	obs_source_t *box1 = obs_source_create(BOX_ID, "box 1", NULL, NULL);
	obs_source_t *box2 = obs_source_create(BOX_ID, "box 2", NULL, NULL);
	assert_non_null(scene);
	assert_non_null(box1);
	assert_non_null(box2);

	obs_set_output_source(0, obs_scene_get_source(scene));

	obs_sceneitem_t *item1 = obs_scene_add(scene, box1);
	obs_sceneitem_t *item2 = obs_scene_add(scene, box2);
	struct vec2 pos;
	vec2_set(&pos, 700.0f, 400.0f);
	obs_sceneitem_set_pos(item1, &pos);
	vec2_set(&pos, 1000.0f, 600.0f);
	obs_sceneitem_set_pos(item2, &pos);

	obs_sceneitem_t *items[] = {item1, item2};
	obs_sceneitem_t *group = obs_scene_insert_group(scene, "group", items, 2);
	assert_non_null(group);
	obs_sceneitem_set_alignment(group, group_align);
	wait_for_ticks();

	struct vec3 start = child_on_canvas(group, item2);

	/* Drag the first child around in fractional steps. */
	for (int i = 0; i < 60; i++) {
		obs_sceneitem_get_pos(item1, &pos);
		pos.x += (i % 2) ? 0.37f : -0.29f;
		pos.y += (i % 3) ? 0.41f : -0.23f;
		obs_sceneitem_set_pos(item1, &pos);
		wait_for_ticks();
	}

	struct vec3 end = child_on_canvas(group, item2);
	float dx = end.x - start.x;
	float dy = end.y - start.y;
	if (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f) {
		fprintf(stderr,
			"%s: the untouched child moved by (%.3f, %.3f) on the canvas, from (%.3f, %.3f) to (%.3f, %.3f)\n",
			what, dx, dy, start.x, start.y, end.x, end.y);
		obs_set_output_source(0, NULL);
		obs_source_remove(obs_scene_get_source(scene));
		obs_scene_release(scene);
		obs_source_release(box1);
		obs_source_release(box2);
		fail();
	}

	obs_set_output_source(0, NULL);
	obs_source_remove(obs_scene_get_source(scene));
	obs_scene_release(scene);
	obs_source_release(box1);
	obs_source_release(box2);
}

static void centered_group_keeps_sibling_in_place(void **state)
{
	(void)state;
	if (!obs_up)
		skip();
	check_sibling_stays(OBS_ALIGN_CENTER, "centre aligned group");
}

static void bottom_right_group_keeps_sibling_in_place(void **state)
{
	(void)state;
	if (!obs_up)
		skip();
	check_sibling_stays(OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT, "bottom-right aligned group");
}

static void top_left_group_keeps_sibling_in_place(void **state)
{
	(void)state;
	if (!obs_up)
		skip();
	check_sibling_stays(OBS_ALIGN_TOP | OBS_ALIGN_LEFT, "top-left aligned group");
}

static int setup(void **state)
{
	(void)state;
	/* obs_startup() needs a display on Linux (run under xvfb-run); the
	 * cases skip without one. */
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		return 0;
	}

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 60,
		.fps_den = 1,
		.base_width = 1920,
		.base_height = 1080,
		.output_width = 1920,
		.output_height = 1080,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	int ret = obs_reset_video(&ovi);
	if (ret != OBS_VIDEO_SUCCESS) {
		fprintf(stderr, "skipped: obs_reset_video failed: %d\n", ret);
		obs_shutdown();
		obs_up = false;
		return 0;
	}

	struct obs_source_info info = {
		.id = BOX_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = box_name,
		.create = box_create,
		.destroy = box_destroy,
		.get_width = box_width,
		.get_height = box_height,
	};
	obs_register_source(&info);
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(centered_group_keeps_sibling_in_place),
		cmocka_unit_test(bottom_right_group_keeps_sibling_in_place),
		cmocka_unit_test(top_left_group_keeps_sibling_in_place),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
