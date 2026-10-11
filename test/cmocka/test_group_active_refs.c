/* Regression test for upstream obsproject/obs-studio#9961 (reported by
 * walker-WSH, also seen by kpaulisse): moving a scene item into or out of a
 * group leaves the source's activate/show counts unbalanced.
 *
 * A visible scene item makes its source an "active child" of the scene that
 * holds it: when that scene is activated, libobs walks its active tree and
 * activates the child too, and deactivation walks the tree again to undo it.
 * Moving an item into a group (or out of one) changes which scene holds it,
 * but libobs never moves the active-child relationship along. The two walks
 * then see different trees, so the counts drift:
 *
 * - into a hidden group: the source keeps the activation it got from the outer
 *   scene; showing the group adds a second one; switching to another scene
 *   removes only one. The source stays active and showing forever, so audio
 *   keeps playing and the mixer keeps listing it after everything is deleted.
 * - out of a hidden group: the source is visible in the program but was never
 *   activated, and switching away drives its counts negative.
 *
 * The frontend moves items with obs_scene_reorder_items2() (drag and drop in
 * the source tree); obs_sceneitem_group_add_item() and
 * obs_sceneitem_group_remove_item() are the API equivalents. All are tested,
 * plus a visible group (the counts must not be doubled), a nested scene, a
 * move between two groups and obs_scene_insert_group() with a hidden item.
 * Every test ends by deleting everything while the program is still on the
 * output, and the raw counts must return to exactly zero.
 *
 * Scenes and groups carry audio, so hiding a group is applied by the audio
 * thread (scene item audio actions). The audio thread only renders the
 * sources of a video mix, so the tests start core video and audio and wait
 * for the group to become inactive. That needs a display, so the test only
 * builds on Linux and runs under xvfb-run.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>

#include <obs-internal.h>
#include <obs-scene.h>
#include <util/platform.h>

#define AUDIO_INPUT_ID "test_group_refs_audio_input"

static int obs_up;

static const char *audio_input_name(void *unused)
{
	(void)unused;
	return "test audio input";
}

static void *audio_input_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void audio_input_destroy(void *data)
{
	(void)data;
}

static void register_audio_input(void)
{
	struct obs_source_info info = {
		.id = AUDIO_INPUT_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_AUDIO,
		.get_name = audio_input_name,
		.create = audio_input_create,
		.destroy = audio_input_destroy,
	};
	obs_register_source(&info);
}

struct fixture {
	obs_scene_t *program;
	obs_scene_t *empty;
	obs_scene_t *nested;
	obs_sceneitem_t *group;
	obs_sceneitem_t *group2;
	obs_sceneitem_t *item;
	obs_sceneitem_t *item2;
	obs_source_t *audio;
	obs_source_t *audio2;
};

static int fixture_setup(void **state)
{
	if (!obs_up) {
		*state = NULL;
		return 0;
	}

	struct fixture *fx = bzalloc(sizeof(*fx));
	fx->program = obs_scene_create("program");
	fx->empty = obs_scene_create("empty");
	fx->nested = obs_scene_create("nested");
	fx->audio = obs_source_create(AUDIO_INPUT_ID, "audio", NULL, NULL);
	fx->audio2 = obs_source_create(AUDIO_INPUT_ID, "audio 2", NULL, NULL);
	if (!fx->program || !fx->empty || !fx->nested || !fx->audio || !fx->audio2)
		return -1;
	*state = fx;
	return 0;
}

static int fixture_teardown(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		return 0;
	obs_set_output_source(0, NULL);
	/* the main canvas holds a reference to its scenes until they are removed */
	obs_source_remove(obs_scene_get_source(fx->program));
	obs_source_remove(obs_scene_get_source(fx->empty));
	obs_source_remove(obs_scene_get_source(fx->nested));
	obs_scene_release(fx->program);
	obs_scene_release(fx->empty);
	obs_scene_release(fx->nested);
	obs_source_release(fx->audio);
	obs_source_release(fx->audio2);
	obs_wait_for_destroy_queue();
	bfree(fx);
	return 0;
}

/* The raw counts, so a doubled activation (2) or an underflow (-1) is caught
 * and not just "active or not". */
static void check_source(obs_source_t *source, long want_active, long want_showing, const char *when)
{
	long active = os_atomic_load_long(&source->activate_refs);
	long showing = os_atomic_load_long(&source->show_refs);
	if (active != want_active || showing != want_showing) {
		fprintf(stderr, "%s: '%s' activate_refs=%ld show_refs=%ld, expected %ld/%ld\n", when,
			obs_source_get_name(source), active, showing, want_active, want_showing);
		fail();
	}
}

static void check_counts(struct fixture *fx, bool want_active, const char *when)
{
	check_source(fx->audio, want_active ? 1 : 0, want_active ? 1 : 0, when);
}

/* Hiding an item whose source has audio is queued as an audio action; wait
 * for the audio thread to apply it. */
static void hide_and_wait(obs_sceneitem_t *item)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	obs_sceneitem_set_visible(item, false);
	for (int i = 0; i < 200 && os_atomic_load_long(&item->active_refs) > 0; i++)
		os_sleep_ms(10);
	if (os_atomic_load_long(&item->active_refs) > 0) {
		fprintf(stderr, "'%s' is still active 2 s after hiding it\n", obs_source_get_name(source));
		fail();
	}
}

static void hide_group(struct fixture *fx)
{
	hide_and_wait(fx->group);
}

static obs_sceneitem_t *add_group(obs_scene_t *scene, const char *name)
{
	obs_sceneitem_t *group = obs_scene_add_group2(scene, name, false);
	if (!group) {
		fprintf(stderr, "could not create group '%s'\n", name);
		fail();
	}
	return group;
}

static void put_group_on_output(struct fixture *fx, bool group_visible)
{
	fx->group = add_group(fx->program, "group");
	obs_set_output_source(0, obs_scene_get_source(fx->program));
	if (!group_visible)
		hide_group(fx);
}

/* Program scene with a group, the audio source added next to the group (not in
 * it), program scene on the output. */
static void build_scene(struct fixture *fx, bool group_visible)
{
	put_group_on_output(fx, group_visible);
	fx->item = obs_scene_add(fx->program, fx->audio);
	check_counts(fx, true, "after adding the source to the program scene");
}

/* Program scene with a group, the audio source inside the group. */
static void build_scene_in_group(struct fixture *fx, bool group_visible)
{
	put_group_on_output(fx, group_visible);
	fx->item = obs_scene_add(obs_sceneitem_group_get_scene(fx->group), fx->audio);
	check_counts(fx, group_visible, "after adding the source to the group");
}

/* What the source tree does when an item is dropped onto a group. */
static void reorder_into_group(struct fixture *fx)
{
	struct obs_sceneitem_order_info order[] = {
		{.group = NULL, .item = fx->group},
		{.group = fx->group, .item = fx->item},
	};
	obs_scene_reorder_items2(fx->program, order, 2);
	if (obs_sceneitem_get_group(fx->program, fx->item) != fx->group) {
		fprintf(stderr, "reorder did not move the item into the group\n");
		fail();
	}
}

static void reorder_out_of_group(struct fixture *fx)
{
	struct obs_sceneitem_order_info order[] = {
		{.group = NULL, .item = fx->group},
		{.group = NULL, .item = fx->item},
	};
	obs_scene_reorder_items2(fx->program, order, 2);
	if (obs_sceneitem_get_group(fx->program, fx->item) != NULL) {
		fprintf(stderr, "reorder did not move the item out of the group\n");
		fail();
	}
}

static bool remove_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	(void)param;
	obs_sceneitem_remove(item);
	return true;
}

/* Delete every item while the program scene is on the output (step 5 of
 * #9961); nothing may stay active or showing. */
static void remove_everything(struct fixture *fx)
{
	obs_set_output_source(0, obs_scene_get_source(fx->program));
	obs_sceneitem_t **items[] = {&fx->item, &fx->item2, &fx->group, &fx->group2};
	for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		if (*items[i])
			obs_sceneitem_remove(*items[i]);
		*items[i] = NULL;
	}
	obs_scene_enum_items(fx->nested, remove_item, NULL);
	check_source(fx->audio, 0, 0, "after deleting everything");
	check_source(fx->audio2, 0, 0, "after deleting everything");
	check_source(obs_scene_get_source(fx->nested), 0, 0, "after deleting everything");
}

/* The steps from #9961: hidden group, move the audio source into it, show the
 * group, switch to an empty scene. */
static void finish_issue_steps(struct fixture *fx)
{
	check_counts(fx, false, "after moving the source into the hidden group");
	obs_sceneitem_set_visible(fx->group, true);
	check_counts(fx, true, "after showing the group");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_counts(fx, false, "after switching to an empty scene");
	remove_everything(fx);
}

static void reorder_into_hidden_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	build_scene(fx, false);
	reorder_into_group(fx);
	finish_issue_steps(fx);
}

static void add_item_to_hidden_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	build_scene(fx, false);
	obs_sceneitem_group_add_item(fx->group, fx->item);
	finish_issue_steps(fx);
}

static void reorder_out_of_hidden_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	build_scene_in_group(fx, false);
	reorder_out_of_group(fx);
	check_counts(fx, true, "after moving the source out of the hidden group");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_counts(fx, false, "after switching to an empty scene");
	remove_everything(fx);
}

static void remove_item_from_hidden_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	build_scene_in_group(fx, false);
	obs_sceneitem_group_remove_item(fx->group, fx->item);
	check_counts(fx, true, "after moving the source out of the hidden group");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_counts(fx, false, "after switching to an empty scene");
	remove_everything(fx);
}

/* With a visible group the counts already balance; the move must not add a
 * second activation. */
static void reorder_into_visible_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	build_scene(fx, true);
	reorder_into_group(fx);
	check_counts(fx, true, "after moving the source into the visible group");
	hide_group(fx);
	check_counts(fx, false, "after hiding the group");
	obs_sceneitem_set_visible(fx->group, true);
	check_counts(fx, true, "after showing the group again");
	obs_sceneitem_group_remove_item(fx->group, fx->item);
	check_counts(fx, true, "after moving the source back out of the visible group");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_counts(fx, false, "after switching to an empty scene");
	remove_everything(fx);
}

/* A hidden item moved into a hidden group holds no activation to move. */
static void add_hidden_item_to_hidden_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	build_scene(fx, false);
	hide_and_wait(fx->item);
	check_counts(fx, false, "after hiding the source");
	obs_sceneitem_group_add_item(fx->group, fx->item);
	check_counts(fx, false, "after moving the hidden source into the hidden group");
	obs_sceneitem_set_visible(fx->group, true);
	check_counts(fx, false, "after showing the group");
	obs_sceneitem_set_visible(fx->item, true);
	check_counts(fx, true, "after showing the source");
	obs_sceneitem_group_remove_item(fx->group, fx->item);
	check_counts(fx, true, "after moving the source back out");
	remove_everything(fx);
}

/* A nested scene carries its whole tree into the group: the audio source two
 * levels down must follow the scene in and out of the hidden group. */
static void reorder_nested_scene_into_hidden_group(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	obs_scene_add(fx->nested, fx->audio);
	put_group_on_output(fx, false);
	fx->item = obs_scene_add(fx->program, obs_scene_get_source(fx->nested));
	check_counts(fx, true, "after adding the nested scene to the program scene");
	reorder_into_group(fx);
	check_counts(fx, false, "after moving the nested scene into the hidden group");
	check_source(obs_scene_get_source(fx->nested), 0, 0, "after moving the nested scene into the hidden group");
	obs_sceneitem_set_visible(fx->group, true);
	check_counts(fx, true, "after showing the group");
	hide_group(fx);
	reorder_out_of_group(fx);
	check_counts(fx, true, "after moving the nested scene back out");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_counts(fx, false, "after switching to an empty scene");
	remove_everything(fx);
}

/* Drag and drop from a visible group straight into a hidden one. */
static void reorder_between_groups(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	fx->group2 = add_group(fx->program, "visible group");
	build_scene_in_group(fx, false);
	fx->item2 = obs_scene_add(fx->program, fx->audio2);
	obs_sceneitem_group_add_item(fx->group2, fx->item2);
	check_source(fx->audio2, 1, 1, "after adding the second source to the visible group");

	/* swap the two sources between the groups */
	struct obs_sceneitem_order_info order[] = {
		{.group = NULL, .item = fx->group},
		{.group = fx->group, .item = fx->item2},
		{.group = NULL, .item = fx->group2},
		{.group = fx->group2, .item = fx->item},
	};
	obs_scene_reorder_items2(fx->program, order, 4);
	check_source(fx->audio, 1, 1, "after moving the first source into the visible group");
	check_source(fx->audio2, 0, 0, "after moving the second source into the hidden group");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_source(fx->audio, 0, 0, "after switching to an empty scene");
	check_source(fx->audio2, 0, 0, "after switching to an empty scene");
	remove_everything(fx);
}

/* Grouping a visible and a hidden source; the group is created visible, so
 * neither count may change. */
static void insert_group_with_hidden_item(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	obs_set_output_source(0, obs_scene_get_source(fx->program));
	fx->item = obs_scene_add(fx->program, fx->audio);
	fx->item2 = obs_scene_add(fx->program, fx->audio2);
	hide_and_wait(fx->item2);
	obs_sceneitem_t *items[] = {fx->item, fx->item2};
	fx->group = obs_scene_insert_group(fx->program, "group", items, 2);
	if (!fx->group) {
		fprintf(stderr, "could not group the sources\n");
		fail();
	}
	check_source(fx->audio, 1, 1, "after grouping the visible source");
	check_source(fx->audio2, 0, 0, "after grouping the hidden source");
	hide_group(fx);
	check_source(fx->audio, 0, 0, "after hiding the group");
	obs_sceneitem_set_visible(fx->group, true);
	obs_sceneitem_set_visible(fx->item2, true);
	check_source(fx->audio, 1, 1, "after showing the group");
	check_source(fx->audio2, 1, 1, "after showing the second source");
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_source(fx->audio, 0, 0, "after switching to an empty scene");
	check_source(fx->audio2, 0, 0, "after switching to an empty scene");
	remove_everything(fx);
}

/* Second report in #9961 (kpaulisse): a scene inside a group of a nested scene,
 * with that nested scene both inside the program scene and shown in the
 * studio mode preview. Hiding the group must deactivate the scene inside it.
 * No item changes group here, and libobs already balances this without the
 * fix; the case guards the preview's extra show reference. */
static void hide_group_in_previewed_nested_scene(void **state)
{
	struct fixture *fx = *state;
	if (!fx)
		skip();
	obs_scene_t *inner = obs_scene_create("inner");
	obs_scene_add(inner, fx->audio);
	obs_source_t *nested = obs_scene_get_source(fx->nested);
	obs_scene_add(fx->nested, fx->audio2);
	fx->group = add_group(fx->nested, "group");
	obs_scene_add(obs_sceneitem_group_get_scene(fx->group), obs_scene_get_source(inner));
	fx->item = obs_scene_add(fx->program, nested);
	obs_set_output_source(0, obs_scene_get_source(fx->program));
	obs_source_inc_showing(nested); /* the studio mode preview */
	check_source(fx->audio, 1, 2, "with the nested scene in program and preview");
	hide_group(fx);
	check_source(fx->audio, 0, 0, "after hiding the group");
	check_source(fx->audio2, 1, 2, "after hiding the group");
	obs_sceneitem_set_visible(fx->group, true);
	check_source(fx->audio, 1, 2, "after showing the group");
	obs_source_dec_showing(nested);
	obs_set_output_source(0, obs_scene_get_source(fx->empty));
	check_source(fx->audio, 0, 0, "after leaving studio mode and switching away");
	check_source(fx->audio2, 0, 0, "after leaving studio mode and switching away");
	fx->group = NULL; /* lives in the nested scene, removed with it */
	remove_everything(fx);
	obs_source_remove(obs_scene_get_source(inner));
	obs_scene_release(inner);
}

static int group_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (obs_up) {
		struct obs_video_info ovi = {
			.graphics_module = GRAPHICS_MODULE_PATH,
			.fps_num = 30,
			.fps_den = 1,
			.base_width = 64,
			.base_height = 64,
			.output_width = 64,
			.output_height = 64,
			.output_format = VIDEO_FORMAT_NV12,
			.colorspace = VIDEO_CS_709,
			.range = VIDEO_RANGE_PARTIAL,
			.scale_type = OBS_SCALE_BICUBIC,
		};
		int ret = obs_reset_video(&ovi);
		if (ret != OBS_VIDEO_SUCCESS) {
			fprintf(stderr, "obs_reset_video failed: %d\n", ret);
			return -1;
		}
		struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
		if (!obs_reset_audio(&oai)) {
			fprintf(stderr, "obs_reset_audio failed\n");
			return -1;
		}
		register_audio_input();
	}
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(reorder_into_visible_group, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(reorder_into_hidden_group, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(add_item_to_hidden_group, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(reorder_out_of_hidden_group, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(remove_item_from_hidden_group, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(add_hidden_item_to_hidden_group, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(reorder_nested_scene_into_hidden_group, fixture_setup,
						fixture_teardown),
		cmocka_unit_test_setup_teardown(reorder_between_groups, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(insert_group_with_hidden_item, fixture_setup, fixture_teardown),
		cmocka_unit_test_setup_teardown(hide_group_in_previewed_nested_scene, fixture_setup, fixture_teardown),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
