/* Regression test for obs-rust/obs-studio#2124: gs_draw_sprite() with
 * GS_FLIP_U must mirror a 2D texture left to right, not turn it upside down.
 *
 * Since upstream 7b8b51b29 ("libobs/graphics: Improve sprite drawing perf")
 * a 2D texture is drawn from a static vertex buffer, plain or flipped on v,
 * and any flip bit picked the v-flipped one. So GS_FLIP_U drew the texture
 * upside down, and GS_FLIP_U | GS_FLIP_V flipped it on v only. PipeWire
 * capture passes GS_FLIP_U for a Flipped or Flipped180 stream.
 *
 * The test draws a 4x4 texture whose every texel has its own colour (red is
 * the column, green the row) into a 4x4 render target with the default
 * effect, for each of the four flips, through gs_draw_sprite(), through a
 * full-size gs_draw_sprite_subregion() (which takes the gs_draw_sprite()
 * shortcut) and through a partial one (built per draw, correct before the
 * fix too), and reads every drawn pixel back.
 *
 * Core video needs a graphics module and libobs needs an X display on Linux,
 * so the cases skip without one (run under xvfb-run). */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <util/platform.h>

#define SIZE 4

static bool obs_up;

static uint8_t texel_r(uint32_t x)
{
	return (uint8_t)(40 + 60 * x);
}

static uint8_t texel_g(uint32_t y)
{
	return (uint8_t)(40 + 60 * y);
}

enum draw_kind {
	DRAW_SPRITE,
	DRAW_SUBREGION_WHOLE,
	DRAW_SUBREGION_PART,
};

static const char *kind_name(enum draw_kind kind)
{
	switch (kind) {
	case DRAW_SPRITE:
		return "gs_draw_sprite";
	case DRAW_SUBREGION_WHOLE:
		return "gs_draw_sprite_subregion (whole)";
	default:
		return "gs_draw_sprite_subregion (part)";
	}
}

/* The partial subregion: texels (1, 1) to (2, 2), drawn 2x2 at the top
 * left. */
#define PART_X 1
#define PART_Y 1
#define PART_CX 2
#define PART_CY 2

/* Draws the texture with flip and returns the 4x4 target as RGBA. */
static void render(enum draw_kind kind, uint32_t flip, uint8_t out[SIZE][SIZE][4])
{
	uint8_t pixels[SIZE * SIZE * 4];
	for (uint32_t y = 0; y < SIZE; y++) {
		for (uint32_t x = 0; x < SIZE; x++) {
			uint8_t *p = pixels + (y * SIZE + x) * 4;
			p[0] = texel_r(x);
			p[1] = texel_g(y);
			p[2] = 0;
			p[3] = 255;
		}
	}
	const uint8_t *data[] = {pixels};

	obs_enter_graphics();
	gs_texture_t *tex = gs_texture_create(SIZE, SIZE, GS_RGBA, 1, data, 0);
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stage = gs_stagesurface_create(SIZE, SIZE, GS_RGBA);
	assert_non_null(tex);
	assert_non_null(tr);
	assert_non_null(stage);

	assert_true(gs_texrender_begin(tr, SIZE, SIZE));
	struct vec4 clear;
	vec4_zero(&clear);
	gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
	gs_ortho(0.0f, (float)SIZE, 0.0f, (float)SIZE, -100.0f, 100.0f);
	gs_blend_state_push();
	gs_reset_blend_state();
	gs_enable_blending(false);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), tex);
	while (gs_effect_loop(effect, "Draw")) {
		switch (kind) {
		case DRAW_SPRITE:
			gs_draw_sprite(tex, flip, 0, 0);
			break;
		case DRAW_SUBREGION_WHOLE:
			gs_draw_sprite_subregion(tex, flip, 0, 0, SIZE, SIZE);
			break;
		case DRAW_SUBREGION_PART:
			gs_draw_sprite_subregion(tex, flip, PART_X, PART_Y, PART_CX, PART_CY);
			break;
		}
	}
	gs_blend_state_pop();
	gs_texrender_end(tr);

	gs_stage_texture(stage, gs_texrender_get_texture(tr));
	uint8_t *mapped;
	uint32_t linesize;
	assert_true(gs_stagesurface_map(stage, &mapped, &linesize));
	for (uint32_t y = 0; y < SIZE; y++)
		memcpy(out[y], mapped + (size_t)y * linesize, SIZE * 4);
	gs_stagesurface_unmap(stage);

	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(tr);
	gs_texture_destroy(tex);
	obs_leave_graphics();
}

/* Every drawn pixel shows the texel that each flip bit picks on its own
 * axis: u mirrors left to right, v top to bottom. */
static void check(enum draw_kind kind, uint32_t flip)
{
	uint8_t out[SIZE][SIZE][4];
	render(kind, flip, out);

	const bool part = kind == DRAW_SUBREGION_PART;
	const uint32_t x0 = part ? PART_X : 0, y0 = part ? PART_Y : 0;
	const uint32_t cx = part ? PART_CX : SIZE, cy = part ? PART_CY : SIZE;
	int bad = 0;
	for (uint32_t y = 0; y < cy; y++) {
		for (uint32_t x = 0; x < cx; x++) {
			uint32_t tx = x0 + ((flip & GS_FLIP_U) ? cx - 1 - x : x);
			uint32_t ty = y0 + ((flip & GS_FLIP_V) ? cy - 1 - y : y);
			const uint8_t *p = out[y][x];
			if (p[0] != texel_r(tx) || p[1] != texel_g(ty) || p[2] != 0 || p[3] != 255) {
				fprintf(stderr,
					"%s flip %u: pixel (%u, %u) is RGBA %u %u %u %u, want texel (%u, %u) = %u %u 0 255\n",
					kind_name(kind), flip, x, y, p[0], p[1], p[2], p[3], tx, ty, texel_r(tx),
					texel_g(ty));
				bad++;
			}
		}
	}
	assert_int_equal(bad, 0);
}

#define FLIP_CASES(kind, prefix)                        \
	static void prefix##_plain(void **state)        \
	{                                               \
		(void)state;                            \
		if (!obs_up)                            \
			skip();                         \
		check(kind, 0);                         \
	}                                               \
	static void prefix##_flip_u(void **state)       \
	{                                               \
		(void)state;                            \
		if (!obs_up)                            \
			skip();                         \
		check(kind, GS_FLIP_U);                 \
	}                                               \
	static void prefix##_flip_v(void **state)       \
	{                                               \
		(void)state;                            \
		if (!obs_up)                            \
			skip();                         \
		check(kind, GS_FLIP_V);                 \
	}                                               \
	static void prefix##_flip_uv(void **state)      \
	{                                               \
		(void)state;                            \
		if (!obs_up)                            \
			skip();                         \
		check(kind, GS_FLIP_U | GS_FLIP_V);     \
	}

FLIP_CASES(DRAW_SPRITE, sprite)
FLIP_CASES(DRAW_SUBREGION_WHOLE, subregion_whole)
FLIP_CASES(DRAW_SUBREGION_PART, subregion_part)

static int group_setup(void **state)
{
	(void)state;
	const char *display = getenv("DISPLAY");
	if (!display || !*display) {
		fprintf(stderr, "skipped: no display\n");
		return 0;
	}
	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "skipped: obs_startup failed\n");
		return 0;
	}

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = SIZE,
		.base_height = SIZE,
		.output_width = SIZE,
		.output_height = SIZE,
		.output_format = VIDEO_FORMAT_BGRA,
		.colorspace = VIDEO_CS_SRGB,
		.range = VIDEO_RANGE_FULL,
		.scale_type = OBS_SCALE_POINT,
	};
	int ret = obs_reset_video(&ovi);
	if (ret != OBS_VIDEO_SUCCESS) {
		fprintf(stderr, "skipped: obs_reset_video failed: %d\n", ret);
		obs_shutdown();
		return 0;
	}
	/* Right after obs_reset_video() a draw can read back an empty target
	 * (every byte 0, no GL error logged), for a few draws and some tens of
	 * milliseconds; after that every draw reads back (Mesa llvmpipe under
	 * Xvfb, the CI device). Not root-caused: the graphics thread starts
	 * rendering frames on the same device then. Probe until three plain
	 * draws in a row read back, for up to 3 s, so every case reads a settled
	 * device. */
	int settled = 0;
	for (int i = 0; i < 300 && settled < 3; i++) {
		uint8_t probe[SIZE][SIZE][4];
		render(DRAW_SPRITE, 0, probe);
		if (probe[0][0][3] == 255) {
			settled++;
		} else {
			settled = 0;
			os_sleep_ms(10);
		}
	}
	if (settled < 3) {
		fprintf(stderr, "a plain draw never read back\n");
		obs_shutdown();
		return -1;
	}
	obs_up = true;
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up) {
		obs_shutdown();
		obs_up = false;
	}
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(sprite_plain),           cmocka_unit_test(sprite_flip_u),
		cmocka_unit_test(sprite_flip_v),          cmocka_unit_test(sprite_flip_uv),
		cmocka_unit_test(subregion_whole_plain),  cmocka_unit_test(subregion_whole_flip_u),
		cmocka_unit_test(subregion_whole_flip_v), cmocka_unit_test(subregion_whole_flip_uv),
		cmocka_unit_test(subregion_part_plain),   cmocka_unit_test(subregion_part_flip_u),
		cmocka_unit_test(subregion_part_flip_v),  cmocka_unit_test(subregion_part_flip_uv),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
