#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cmocka.h>

#include <graphics/effect.h>
#include <util/bmem.h>

/*
 * Characterization of the effect name and index lookups in
 * libobs/graphics/effect.c.h (moved to effect-lookups.c.h in the same
 * change), before the port to obs-graphics (#1822).
 *
 * The test builds a gs_effect by hand and never starts a graphics device.
 * Name walks are a first-match strcmp. Param indexes stop at the count
 * (index >= num returns NULL). Annotation indexes and pass indexes use >,
 * so an index equal to the count returns one past the array. Those returns
 * are characterized, not endorsed: the test compares the pointer and does
 * not dereference it.
 */

static void add_param(gs_effect_t *effect, const char *name, enum gs_shader_param_type type)
{
	struct gs_effect_param param;

	effect_param_init(&param);
	param.name = bstrdup(name);
	param.type = type;
	da_push_back(effect->params, &param);
}

static void add_annotation(gs_eparam_t *param, const char *name, enum gs_shader_param_type type)
{
	struct gs_effect_param annotation;

	effect_param_init(&annotation);
	annotation.name = bstrdup(name);
	annotation.type = type;
	da_push_back(param->annotations, &annotation);
}

static void add_technique(gs_effect_t *effect, const char *name)
{
	struct gs_effect_technique technique;

	effect_technique_init(&technique);
	technique.name = bstrdup(name);
	technique.effect = effect;
	da_push_back(effect->techniques, &technique);
}

static void add_pass(gs_technique_t *technique, const char *name)
{
	struct gs_effect_pass pass;

	effect_pass_init(&pass);
	pass.name = bstrdup(name);
	da_push_back(technique->passes, &pass);
}

/* Frees only what this file allocated. effect_free() would also destroy
 * shaders, and that wants a graphics context. */
static void free_effect(gs_effect_t *effect)
{
	size_t i, j;

	for (i = 0; i < effect->params.num; i++) {
		struct gs_effect_param *param = effect->params.array + i;

		for (j = 0; j < param->annotations.num; j++)
			bfree(param->annotations.array[j].name);
		da_free(param->annotations);
		bfree(param->name);
	}
	da_free(effect->params);

	for (i = 0; i < effect->techniques.num; i++) {
		struct gs_effect_technique *technique = effect->techniques.array + i;

		for (j = 0; j < technique->passes.num; j++)
			bfree(technique->passes.array[j].name);
		da_free(technique->passes);
		bfree(technique->name);
	}
	da_free(effect->techniques);
}

static void null_and_empty_test(void **state)
{
	struct gs_effect_param_info info;
	gs_effect_t effect;
	long allocs;

	UNUSED_PARAMETER(state);
	allocs = bnum_allocs();

	assert_null(gs_effect_get_technique(NULL, "Draw"));
	assert_null(gs_effect_get_current_technique(NULL));
	assert_int_equal(gs_effect_get_num_params(NULL), 0);
	assert_null(gs_effect_get_param_by_idx(NULL, 0));
	assert_null(gs_effect_get_param_by_name(NULL, "color"));
	assert_null(gs_effect_get_viewproj_matrix(NULL));
	assert_null(gs_effect_get_world_matrix(NULL));

	assert_int_equal(gs_param_get_num_annotations(NULL), 0);
	assert_null(gs_param_get_annotation_by_idx(NULL, 0));
	assert_null(gs_param_get_annotation_by_name(NULL, "widget"));
	assert_null(gs_technique_get_pass_by_idx(NULL, 0));
	assert_null(gs_technique_get_pass_by_name(NULL, "p0"));

	info.name = "sentinel";
	info.type = GS_SHADER_PARAM_TEXTURE;
	gs_effect_get_param_info(NULL, &info);
	assert_string_equal(info.name, "sentinel");
	assert_int_equal(info.type, GS_SHADER_PARAM_TEXTURE);

	effect_init(&effect);
	assert_int_equal(gs_effect_get_num_params(&effect), 0);
	assert_null(gs_effect_get_param_by_idx(&effect, 0));
	assert_null(gs_effect_get_param_by_name(&effect, "color"));
	assert_null(gs_effect_get_technique(&effect, "Draw"));
	assert_null(gs_effect_get_current_technique(&effect));
	assert_null(gs_effect_get_viewproj_matrix(&effect));
	assert_null(gs_effect_get_world_matrix(&effect));

	assert_int_equal(bnum_allocs(), allocs);
}

static void name_lookups_test(void **state)
{
	gs_effect_t effect;
	gs_eparam_t *color;
	gs_eparam_t *color_upper;
	gs_eparam_t *color_again;
	gs_eparam_t *empty_param;
	gs_technique_t *draw;
	gs_technique_t *draw_again;
	gs_technique_t *solid;
	gs_technique_t *empty_tech;
	long allocs;

	UNUSED_PARAMETER(state);
	allocs = bnum_allocs();
	effect_init(&effect);

	add_param(&effect, "color", GS_SHADER_PARAM_VEC4);
	add_param(&effect, "Color", GS_SHADER_PARAM_FLOAT);
	add_param(&effect, "color", GS_SHADER_PARAM_INT);
	add_param(&effect, "", GS_SHADER_PARAM_STRING);
	color = effect.params.array + 0;
	color_upper = effect.params.array + 1;
	color_again = effect.params.array + 2;
	empty_param = effect.params.array + 3;

	add_annotation(color, "ui", GS_SHADER_PARAM_STRING);
	add_annotation(color, "ui", GS_SHADER_PARAM_BOOL);
	add_annotation(color, "widget", GS_SHADER_PARAM_STRING);

	add_technique(&effect, "Draw");
	add_technique(&effect, "Draw");
	add_technique(&effect, "Solid");
	add_technique(&effect, "");
	draw = effect.techniques.array + 0;
	draw_again = effect.techniques.array + 1;
	solid = effect.techniques.array + 2;
	empty_tech = effect.techniques.array + 3;

	add_pass(draw, "p0");
	add_pass(draw, "P0");
	add_pass(draw, "p0");

	assert_ptr_equal(gs_effect_get_param_by_name(&effect, "color"), color);
	assert_ptr_not_equal(gs_effect_get_param_by_name(&effect, "color"), color_again);
	assert_ptr_equal(gs_effect_get_param_by_name(&effect, "Color"), color_upper);
	assert_ptr_equal(gs_effect_get_param_by_name(&effect, ""), empty_param);
	assert_null(gs_effect_get_param_by_name(&effect, "col"));
	assert_null(gs_effect_get_param_by_name(&effect, "missing"));

	assert_ptr_equal(gs_param_get_annotation_by_name(color, "ui"), color->annotations.array + 0);
	assert_ptr_not_equal(gs_param_get_annotation_by_name(color, "ui"), color->annotations.array + 1);
	assert_ptr_equal(gs_param_get_annotation_by_name(color, "widget"), color->annotations.array + 2);
	assert_null(gs_param_get_annotation_by_name(color, "UI"));
	assert_null(gs_param_get_annotation_by_name(color, "color"));
	assert_null(gs_param_get_annotation_by_name(color_upper, "ui"));

	assert_ptr_equal(gs_effect_get_technique(&effect, "Draw"), draw);
	assert_ptr_not_equal(gs_effect_get_technique(&effect, "Draw"), draw_again);
	assert_ptr_equal(gs_effect_get_technique(&effect, "Solid"), solid);
	assert_ptr_equal(gs_effect_get_technique(&effect, ""), empty_tech);
	assert_null(gs_effect_get_technique(&effect, "draw"));
	assert_null(gs_effect_get_technique(&effect, "missing"));

	assert_ptr_equal(gs_technique_get_pass_by_name(draw, "p0"), draw->passes.array + 0);
	assert_ptr_not_equal(gs_technique_get_pass_by_name(draw, "p0"), draw->passes.array + 2);
	assert_ptr_equal(gs_technique_get_pass_by_name(draw, "P0"), draw->passes.array + 1);
	assert_null(gs_technique_get_pass_by_name(draw, "p1"));
	assert_null(gs_technique_get_pass_by_name(solid, "p0"));

	free_effect(&effect);
	assert_int_equal(bnum_allocs(), allocs);
}

static void index_bounds_test(void **state)
{
	gs_effect_t effect;
	gs_eparam_t *annotated;
	gs_eparam_t *bare;
	gs_technique_t *with_passes;
	gs_technique_t *without_passes;
	long allocs;

	UNUSED_PARAMETER(state);
	allocs = bnum_allocs();
	effect_init(&effect);

	add_param(&effect, "color", GS_SHADER_PARAM_VEC4);
	add_param(&effect, "tint", GS_SHADER_PARAM_VEC4);
	annotated = effect.params.array + 0;
	bare = effect.params.array + 1;
	add_annotation(annotated, "min", GS_SHADER_PARAM_FLOAT);
	add_annotation(annotated, "max", GS_SHADER_PARAM_FLOAT);

	add_technique(&effect, "Draw");
	add_technique(&effect, "Solid");
	with_passes = effect.techniques.array + 0;
	without_passes = effect.techniques.array + 1;
	add_pass(with_passes, "p0");
	add_pass(with_passes, "p1");

	assert_int_equal(gs_effect_get_num_params(&effect), 2);
	assert_ptr_equal(gs_effect_get_param_by_idx(&effect, 0), annotated);
	assert_ptr_equal(gs_effect_get_param_by_idx(&effect, 1), bare);
	assert_null(gs_effect_get_param_by_idx(&effect, 2));
	assert_null(gs_effect_get_param_by_idx(&effect, 3));

	assert_int_equal(gs_param_get_num_annotations(annotated), 2);
	assert_ptr_equal(gs_param_get_annotation_by_idx(annotated, 0), annotated->annotations.array + 0);
	assert_ptr_equal(gs_param_get_annotation_by_idx(annotated, 1), annotated->annotations.array + 1);
	/* characterized, not endorsed: index == num returns one past the array */
	assert_ptr_equal(gs_param_get_annotation_by_idx(annotated, 2), annotated->annotations.array + 2);
	assert_null(gs_param_get_annotation_by_idx(annotated, 3));

	assert_int_equal(gs_param_get_num_annotations(bare), 0);
	assert_null(gs_param_get_annotation_by_idx(bare, 0));
	assert_null(gs_param_get_annotation_by_idx(bare, 1));

	assert_ptr_equal(gs_technique_get_pass_by_idx(with_passes, 0), with_passes->passes.array + 0);
	assert_ptr_equal(gs_technique_get_pass_by_idx(with_passes, 1), with_passes->passes.array + 1);
	/* characterized, not endorsed: index == num returns one past the array */
	assert_ptr_equal(gs_technique_get_pass_by_idx(with_passes, 2), with_passes->passes.array + 2);
	assert_null(gs_technique_get_pass_by_idx(with_passes, 3));

	assert_null(gs_technique_get_pass_by_idx(without_passes, 0));
	assert_null(gs_technique_get_pass_by_idx(without_passes, 1));

	free_effect(&effect);
	assert_int_equal(bnum_allocs(), allocs);
}

static void stored_pointers_test(void **state)
{
	gs_effect_t effect;
	struct gs_effect_technique outsider;
	long allocs;

	UNUSED_PARAMETER(state);
	allocs = bnum_allocs();
	effect_init(&effect);
	add_param(&effect, "ViewProj", GS_SHADER_PARAM_MATRIX4X4);
	add_param(&effect, "World", GS_SHADER_PARAM_MATRIX4X4);
	add_technique(&effect, "Draw");

	effect.view_proj = effect.params.array + 0;
	effect.world = effect.params.array + 1;
	assert_ptr_equal(gs_effect_get_viewproj_matrix(&effect), effect.params.array + 0);
	assert_ptr_equal(gs_effect_get_world_matrix(&effect), effect.params.array + 1);

	effect.cur_technique = effect.techniques.array;
	assert_ptr_equal(gs_effect_get_current_technique(&effect), effect.techniques.array);

	effect_technique_init(&outsider);
	outsider.name = bstrdup("outsider");
	effect.cur_technique = &outsider;
	assert_ptr_equal(gs_effect_get_current_technique(&effect), &outsider);

	effect.cur_technique = NULL;
	assert_null(gs_effect_get_current_technique(&effect));

	bfree(outsider.name);
	free_effect(&effect);
	assert_int_equal(bnum_allocs(), allocs);
}

static void param_info_test(void **state)
{
	gs_effect_t effect;
	gs_eparam_t *color;
	struct gs_effect_param_info info;
	long allocs;

	UNUSED_PARAMETER(state);
	allocs = bnum_allocs();
	effect_init(&effect);
	add_param(&effect, "color", GS_SHADER_PARAM_VEC4);
	color = effect.params.array;

	info.name = "sentinel";
	info.type = GS_SHADER_PARAM_TEXTURE;
	gs_effect_get_param_info(NULL, &info);
	assert_string_equal(info.name, "sentinel");
	assert_int_equal(info.type, GS_SHADER_PARAM_TEXTURE);

	gs_effect_get_param_info(color, &info);
	assert_ptr_equal(info.name, color->name);
	assert_string_equal(info.name, "color");
	assert_int_equal(info.type, GS_SHADER_PARAM_VEC4);

	free_effect(&effect);
	assert_int_equal(bnum_allocs(), allocs);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(null_and_empty_test), cmocka_unit_test(name_lookups_test),
		cmocka_unit_test(index_bounds_test),   cmocka_unit_test(stored_pointers_test),
		cmocka_unit_test(param_info_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
