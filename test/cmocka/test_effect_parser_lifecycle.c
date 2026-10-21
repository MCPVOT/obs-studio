#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <graphics/effect-parser.h>

/* Original-C, source-only characterization of ep_free (#1644). The symbol
 * is hidden in libobs, so this executable compiles the extracted C unit.
 * OFF/ON changes its libobs dependencies, not the implementation under test.
 * No device or obs_startup is needed. NULL/uninitialized parsers and aliased
 * owned allocations are outside the C routine's contract and are not tested.
 */

#define assert_empty(value)                     \
	do {                                   \
		assert_null((value).array);      \
		assert_int_equal((value).num, 0); \
		assert_int_equal((value).capacity, 0); \
	} while (0)

static void assert_cleared(struct effect_parser *ep)
{
	assert_empty(ep->params);
	assert_empty(ep->structs);
	assert_empty(ep->funcs);
	assert_empty(ep->samplers);
	assert_empty(ep->techniques);
	assert_null(ep->cur_pass);
	assert_null(ep->cfp.cur_token);
	assert_empty(ep->cfp.lex.tokens);
	assert_empty(ep->cfp.pp.tokens);
	assert_empty(ep->cfp.pp.defines);
	assert_empty(ep->cfp.pp.dependencies);
	assert_empty(ep->cfp.pp.sys_include_dirs);
	assert_empty(ep->cfp.error_list.errors);
}

static void finish(struct effect_parser *ep, long allocations)
{
	ep_free(ep);
	assert_cleared(ep);
	assert_int_equal(bnum_allocs(), allocations);
	/* Repeated cleanup is supported, including after populated cleanup. */
	ep_free(ep);
	assert_cleared(ep);
	assert_int_equal(bnum_allocs(), allocations);
}

static void empty_cleanup(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	finish(&ep, allocations);
}

static struct ep_param make_param(const char *name)
{
	struct ep_param param = {0};
	ep_param_init(&param, bstrdup("float"), bstrdup(name), false, false, true);
	uint8_t value[] = {1, 2, 3, 4};
	da_push_back_array(param.default_val, value, sizeof(value));
	return param;
}

static void nested_parameters(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	for (size_t i = 0; i < 2; i++) {
		struct ep_param param = make_param("parameter");
		struct ep_param annotation = make_param("annotation");
		struct ep_param nested = make_param("nested");
		da_push_back(annotation.annotations, &nested);
		da_push_back(param.annotations, &annotation);
		da_push_back(ep.params, &param);
	}
	finish(&ep, allocations);
}

static struct ep_var make_var(void)
{
	struct ep_var var;
	ep_var_init(&var);
	var.type = bstrdup("float4");
	var.name = bstrdup("position");
	var.mapping = bstrdup("POSITION");
	return var;
}

static void structures(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	for (size_t i = 0; i < 2; i++) {
		struct ep_struct item;
		ep_struct_init(&item);
		item.name = bstrdup("vertex");
		for (size_t j = 0; j < 2; j++) {
			struct ep_var var = make_var();
			da_push_back(item.vars, &var);
		}
		da_push_back(ep.structs, &item);
	}
	finish(&ep, allocations);
}

static void functions_and_borrowed_dependencies(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	char *dependency = bstrdup("borrowed dependency");
	struct effect_parser ep = {0};
	ep_init(&ep);
	for (size_t i = 0; i < 2; i++) {
		struct ep_func func;
		ep_func_init(&func, bstrdup("float4"), bstrdup("main"));
		func.mapping = bstrdup("TARGET");
		dstr_copy(&func.contents, "return position;");
		for (size_t j = 0; j < 2; j++) {
			struct ep_var var = make_var();
			da_push_back(func.param_vars, &var);
			da_push_back(func.func_deps, &dependency);
			da_push_back(func.struct_deps, &dependency);
			da_push_back(func.param_deps, &dependency);
			da_push_back(func.sampler_deps, &dependency);
		}
		da_push_back(ep.funcs, &func);
	}
	finish(&ep, allocations + 1);
	assert_string_equal(dependency, "borrowed dependency");
	bfree(dependency);
	assert_int_equal(bnum_allocs(), allocations);
}

static void samplers(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	for (size_t i = 0; i < 2; i++) {
		struct ep_sampler sampler;
		ep_sampler_init(&sampler);
		sampler.name = bstrdup("linear_sampler");
		for (size_t j = 0; j < 2; j++) {
			char *name = bstrdup("Filter");
			char *value = bstrdup("Linear");
			da_push_back(sampler.states, &name);
			da_push_back(sampler.values, &value);
		}
		da_push_back(ep.samplers, &sampler);
	}
	finish(&ep, allocations);
}

static void techniques_and_passes(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	struct gs_effect_pass *borrowed_pass = bzalloc(1);
	ep.cur_pass = borrowed_pass;
	for (size_t i = 0; i < 2; i++) {
		struct ep_technique technique;
		ep_technique_init(&technique);
		technique.name = bstrdup("Draw");
		for (size_t j = 0; j < 2; j++) {
			struct ep_pass pass;
			ep_pass_init(&pass);
			pass.name = bstrdup("pass");
			pass.pass = borrowed_pass;
			struct cf_token token = {0};
			da_push_back(pass.vertex_program, &token);
			da_push_back(pass.fragment_program, &token);
			da_push_back(technique.passes, &pass);
		}
		da_push_back(ep.techniques, &technique);
	}
	finish(&ep, allocations + 1);
	assert_int_equal(*(uint8_t *)borrowed_pass, 0);
	bfree(borrowed_pass);
	assert_int_equal(bnum_allocs(), allocations);
}

static void parser_state(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	assert_true(cf_parser_parse(&ep.cfp, "#define VALUE 2\nfloat value = VALUE;\n", "lifecycle.effect"));
	cf_preprocessor_add_sys_include_dir(&ep.cfp.pp, "unused-include-directory");
	struct cf_lexer dependency;
	cf_lexer_init(&dependency);
	assert_true(cf_lexer_lex(&dependency, "float dependency;", "dependency.effect"));
	da_push_back(ep.cfp.pp.dependencies, &dependency);
	cf_adderror(&ep.cfp, "characterized diagnostic", LEX_ERROR, NULL, NULL, NULL);
	assert_true(ep.cfp.lex.tokens.num > 0);
	assert_true(ep.cfp.pp.tokens.num > 0);
	assert_true(ep.cfp.pp.defines.num > 0);
	assert_int_equal(ep.cfp.error_list.errors.num, 1);
	finish(&ep, allocations);
}

static void retained_fields_and_borrowed_properties(void **state)
{
	(void)state;
	long allocations = bnum_allocs();
	struct effect_parser ep = {0};
	ep_init(&ep);
	gs_effect_t *effect = bzalloc(1);
	struct gs_effect_param *effect_param = bzalloc(1);
	char *property = bstrdup("borrowed property");
	ep.effect = effect;
	struct ep_param param = make_param("parameter");
	param.param = effect_param;
	da_push_back(param.properties, &property);
	da_push_back(ep.params, &param);
	/* Characterized, not endorsed: the unused top-level files/tokens fields
	 * are untouched by ep_free. They are distinct from cfp's owned arrays. */
	struct cf_lexer file;
	cf_lexer_init(&file);
	struct cf_token token = {0};
	da_push_back(ep.files, &file);
	da_push_back(ep.tokens, &token);
	struct cf_lexer *files = ep.files.array;
	struct cf_token *tokens = ep.tokens.array;
	ep_free(&ep);
	assert_cleared(&ep);
	assert_ptr_equal(ep.effect, effect);
	assert_ptr_equal(ep.files.array, files);
	assert_int_equal(ep.files.num, 1);
	assert_ptr_equal(ep.tokens.array, tokens);
	assert_int_equal(ep.tokens.num, 1);
	assert_string_equal(property, "borrowed property");
	assert_int_equal(*(uint8_t *)effect, 0);
	assert_int_equal(*(uint8_t *)effect_param, 0);
	da_free(ep.files);
	da_free(ep.tokens);
	bfree(property);
	bfree(effect_param);
	bfree(effect);
	assert_int_equal(bnum_allocs(), allocations);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(empty_cleanup), cmocka_unit_test(nested_parameters),
		cmocka_unit_test(structures),    cmocka_unit_test(functions_and_borrowed_dependencies),
		cmocka_unit_test(samplers),      cmocka_unit_test(techniques_and_passes),
		cmocka_unit_test(parser_state),  cmocka_unit_test(retained_fields_and_borrowed_properties),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
