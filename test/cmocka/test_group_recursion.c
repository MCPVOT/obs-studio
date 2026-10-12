/* A scene could be moved into a group that the scene itself contains,
 * making the scene contain itself: OBS then recursed until it crashed
 * (obsproject/obs-studio#10466, reported by Penwy). In the report: scene A
 * holds group G; scene B holds both A and G; dragging A into G in B's
 * source list crashes.
 *
 * obs_scene_add() refuses such a cycle through obs_source_add_active_child(),
 * but moving an existing item into a group (obs_sceneitem_group_add_item,
 * and obs_scene_reorder_items2, which the source list's drag and drop uses)
 * went through move_active_child(), which skipped the check for inactive
 * items and ignored its result otherwise.
 *
 * Each case runs in a child process with its own libobs, so a crash or a
 * hang fails the case instead of the whole test. obs_startup() needs a
 * display on Linux, so the cases skip without one (run under xvfb-run). */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <obs.h>
#include <util/base.h>

#define BOX_ID "group_recursion_box"
#define CHILD_SKIP 77
#define CHILD_TIMEOUT 30

static void quiet_log(int lvl, const char *msg, va_list args, void *p)
{
	(void)lvl;
	(void)msg;
	(void)args;
	(void)p;
}

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

static uint32_t box_size(void *data)
{
	(void)data;
	return 64;
}

static bool start_obs(void)
{
	base_set_log_handler(quiet_log, NULL);
	if (!obs_startup("en-US", NULL, NULL))
		return false;

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = 640,
		.base_height = 360,
		.output_width = 640,
		.output_height = 360,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return false;

	struct obs_source_info info = {
		.id = BOX_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = box_name,
		.create = box_create,
		.destroy = box_destroy,
		.get_width = box_size,
		.get_height = box_size,
	};
	obs_register_source(&info);
	return true;
}

/* The scenes from the report. */
struct layout {
	obs_scene_t *a;
	obs_scene_t *b;
	obs_sceneitem_t *a_in_b; /* scene A as an item of B */
	obs_sceneitem_t *g_in_b; /* group G (which lives in A) as an item of B */
	obs_sceneitem_t *box_in_b;
};

static void build_layout(struct layout *l)
{
	l->a = obs_scene_create("scene A");
	obs_sceneitem_t *g_in_a = obs_scene_add_group(l->a, "group G");
	l->b = obs_scene_create("scene B");
	l->a_in_b = obs_scene_add(l->b, obs_scene_get_source(l->a));
	l->g_in_b = obs_scene_add(l->b, obs_sceneitem_get_source(g_in_a));
	obs_source_t *box = obs_source_create(BOX_ID, "box", NULL, NULL);
	l->box_in_b = obs_scene_add(l->b, box);
	obs_source_release(box);
}

/* Child exit codes: 0 ok, 1 failed (message on stderr), CHILD_SKIP. */
typedef int (*scenario_fn)(void);

static void run_in_child(scenario_fn fn, const char *what)
{
	fflush(stderr);
	pid_t pid = fork();
	assert_true(pid >= 0);
	if (pid == 0) {
		alarm(CHILD_TIMEOUT);
		if (!start_obs())
			_exit(CHILD_SKIP);
		int ret = fn();
		fflush(stderr);
		_exit(ret);
	}

	int status = 0;
	assert_int_equal(waitpid(pid, &status, 0), pid);
	if (WIFEXITED(status) && WEXITSTATUS(status) == CHILD_SKIP)
		skip();
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "%s: the child died from signal %d (%s)\n", what, WTERMSIG(status),
			WTERMSIG(status) == SIGALRM ? "hang" : "crash");
		fail();
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "%s failed\n", what);
		fail();
	}
}

/* Scene A must not end up inside group G, and asking for A's size (which
 * walks its tree) must return. */
static int check_no_cycle(struct layout *l, const char *what)
{
	if (obs_sceneitem_get_scene(l->a_in_b) != l->b) {
		fprintf(stderr, "%s: scene A was moved into group G, which scene A contains\n", what);
		return 1;
	}
	(void)obs_source_get_width(obs_scene_get_source(l->a));
	(void)obs_source_get_width(obs_scene_get_source(l->b));
	return 0;
}

static int scenario_group_add_item(void)
{
	struct layout l;
	build_layout(&l);
	obs_sceneitem_group_add_item(l.g_in_b, l.a_in_b);
	return check_no_cycle(&l, "obs_sceneitem_group_add_item");
}

static int scenario_reorder_into_group(void)
{
	struct layout l;
	build_layout(&l);
	/* drag and drop: A placed under G in B's source list */
	struct obs_sceneitem_order_info order[] = {
		{NULL, l.g_in_b},
		{l.g_in_b, l.a_in_b},
		{NULL, l.box_in_b},
	};
	if (obs_scene_reorder_items2(l.b, order, 3)) {
		fprintf(stderr, "obs_scene_reorder_items2 accepted an order that puts scene A inside group G\n");
		return 1;
	}
	return check_no_cycle(&l, "obs_scene_reorder_items2");
}

/* Guard: moving an ordinary source into the group still works. */
static int scenario_box_into_group(void)
{
	struct layout l;
	build_layout(&l);
	obs_scene_t *g_scene = obs_sceneitem_group_get_scene(l.g_in_b);

	struct obs_sceneitem_order_info order[] = {
		{NULL, l.a_in_b},
		{NULL, l.g_in_b},
		{l.g_in_b, l.box_in_b},
	};
	if (!obs_scene_reorder_items2(l.b, order, 3) || obs_sceneitem_get_scene(l.box_in_b) != g_scene) {
		fprintf(stderr, "a source was not moved into the group by obs_scene_reorder_items2\n");
		return 1;
	}

	obs_source_t *box = obs_source_create(BOX_ID, "box 2", NULL, NULL);
	obs_sceneitem_t *box2 = obs_scene_add(l.b, box);
	obs_source_release(box);
	obs_sceneitem_group_add_item(l.g_in_b, box2);
	if (obs_sceneitem_get_scene(box2) != g_scene) {
		fprintf(stderr, "a source was not moved into the group by obs_sceneitem_group_add_item\n");
		return 1;
	}
	return 0;
}

static void group_add_item_refuses_cycle(void **state)
{
	(void)state;
	run_in_child(scenario_group_add_item, "obs_sceneitem_group_add_item");
}

static void reorder_refuses_cycle(void **state)
{
	(void)state;
	run_in_child(scenario_reorder_into_group, "obs_scene_reorder_items2");
}

static void ordinary_source_moves_into_group(void **state)
{
	(void)state;
	run_in_child(scenario_box_into_group, "moving a source into a group");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(group_add_item_refuses_cycle),
		cmocka_unit_test(reorder_refuses_cycle),
		cmocka_unit_test(ordinary_source_moves_into_group),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
