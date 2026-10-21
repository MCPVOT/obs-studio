/* Original cleanup and production inline adapters use the original C
 * lexer/preprocessor dependencies in the differential test executable. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ep_free oracle_ep_free
#define cf_lexer_init oracle_cf_lexer_init
#define cf_lexer_free oracle_cf_lexer_free
#define cf_lexer_lex oracle_cf_lexer_lex
#define cf_preprocessor_init oracle_cf_preprocessor_init
#define cf_preprocessor_free oracle_cf_preprocessor_free
#define cf_preprocess oracle_cf_preprocess
#define cf_adderror oracle_cf_adderror
#include "graphics/effect-parser.h"
/* build.rs copies both bodies verbatim, excluding only the unrelated native
 * PCH dependency setup. No cleanup operations are reimplemented here. */
#include "effect-parser-lifecycle.inc"
#include "effect-parser-lifecycle-adapter.inc"

size_t oracle_effect_parser_layout(size_t index);
void *oracle_effect_parser_fixture(unsigned seed);
unsigned oracle_effect_parser_check(void *fixture);

size_t oracle_effect_parser_layout(size_t index)
{
	const size_t values[] = {
		sizeof(struct effect_parser),
		_Alignof(struct effect_parser),
		offsetof(struct effect_parser, effect),
		offsetof(struct effect_parser, params),
		offsetof(struct effect_parser, structs),
		offsetof(struct effect_parser, funcs),
		offsetof(struct effect_parser, samplers),
		offsetof(struct effect_parser, techniques),
		offsetof(struct effect_parser, files),
		offsetof(struct effect_parser, tokens),
		offsetof(struct effect_parser, cur_pass),
		offsetof(struct effect_parser, cfp),
	};
	return values[index];
}

struct lifecycle_fixture {
	struct effect_parser ep;
	char *borrowed;
	long before;
	struct cf_lexer *files;
	struct cf_token *tokens;
};

static struct ep_param lifecycle_param(unsigned depth, char *borrowed)
{
	struct ep_param param = {0};
	ep_param_init(&param, bstrdup("float"), bstrdup("value"), false, false, true);
	const uint8_t value[] = {1, 2, 3, 4};
	da_push_back_array(param.default_val, value, sizeof(value));
	da_push_back(param.properties, &borrowed);
	param.param = (struct gs_effect_param *)borrowed;
	if (depth) {
		struct ep_param child = lifecycle_param(depth - 1, borrowed);
		da_push_back(param.annotations, &child);
	}
	return param;
}

static struct ep_var lifecycle_var(void)
{
	struct ep_var var;
	ep_var_init(&var);
	var.type = bstrdup("float4");
	var.name = bstrdup("position");
	var.mapping = bstrdup("POSITION");
	return var;
}

void *oracle_effect_parser_fixture(unsigned seed)
{
	long before = bnum_allocs();
	struct lifecycle_fixture *f = bzalloc(sizeof(*f));
	f->before = before;
	f->borrowed = bstrdup("borrowed");
	ep_init(&f->ep);
	f->ep.effect = (gs_effect_t *)f->borrowed;
	f->ep.cur_pass = (struct gs_effect_pass *)f->borrowed;
	struct cf_lexer file;
	cf_lexer_init(&file);
	struct cf_token token = {0};
	da_push_back(f->ep.files, &file);
	da_push_back(f->ep.tokens, &token);
	f->files = f->ep.files.array;
	f->tokens = f->ep.tokens.array;

	for (unsigned i = 0; i < seed % 5; i++) {
		struct ep_param param = lifecycle_param((seed >> 3) % 4, f->borrowed);
		da_push_back(f->ep.params, &param);
	}
	for (unsigned i = 0; i < (seed >> 5) % 5; i++) {
		struct ep_struct item;
		ep_struct_init(&item);
		item.name = bstrdup("structure");
		for (unsigned j = 0; j < (seed >> 8) % 4; j++) {
			struct ep_var var = lifecycle_var();
			da_push_back(item.vars, &var);
		}
		da_push_back(f->ep.structs, &item);
	}
	for (unsigned i = 0; i < (seed >> 10) % 5; i++) {
		struct ep_func func;
		ep_func_init(&func, bstrdup("float4"), bstrdup("main"));
		func.mapping = bstrdup("TARGET");
		dstr_copy(&func.contents, "return position;");
		for (unsigned j = 0; j < (seed >> 13) % 4; j++) {
			struct ep_var var = lifecycle_var();
			da_push_back(func.param_vars, &var);
			da_push_back(func.func_deps, &f->borrowed);
			da_push_back(func.struct_deps, &f->borrowed);
			da_push_back(func.param_deps, &f->borrowed);
			da_push_back(func.sampler_deps, &f->borrowed);
		}
		da_push_back(f->ep.funcs, &func);
	}
	for (unsigned i = 0; i < (seed >> 15) % 5; i++) {
		struct ep_sampler sampler;
		ep_sampler_init(&sampler);
		sampler.name = bstrdup("sampler");
		for (unsigned j = 0; j < (seed >> 18) % 4; j++) {
			char *state = bstrdup("Filter");
			char *value = bstrdup("Linear");
			da_push_back(sampler.states, &state);
			da_push_back(sampler.values, &value);
		}
		da_push_back(f->ep.samplers, &sampler);
	}
	for (unsigned i = 0; i < (seed >> 20) % 5; i++) {
		struct ep_technique technique;
		ep_technique_init(&technique);
		technique.name = bstrdup("Draw");
		for (unsigned j = 0; j < (seed >> 23) % 4; j++) {
			struct ep_pass pass;
			ep_pass_init(&pass);
			pass.name = bstrdup("pass");
			pass.pass = (struct gs_effect_pass *)f->borrowed;
			da_push_back(pass.vertex_program, &token);
			da_push_back(pass.fragment_program, &token);
			da_push_back(technique.passes, &pass);
		}
		da_push_back(f->ep.techniques, &technique);
	}
	if (seed & (1U << 27)) {
		bool parsed = cf_parser_parse(&f->ep.cfp, "#define VALUE 2\nfloat value = VALUE;\n", "fixture.effect");
		assert(parsed);
		(void)parsed;
		cf_preprocessor_add_sys_include_dir(&f->ep.cfp.pp, "unused");
		cf_adderror(&f->ep.cfp, "diagnostic", LEX_ERROR, NULL, NULL, NULL);
	}
	return f;
}

/* A zero result means all characterized postconditions and allocation
 * accounting match. Only initialized, nonaliasing owned graphs are generated. */
unsigned oracle_effect_parser_check(void *fixture)
{
	struct lifecycle_fixture *f = fixture;
	struct effect_parser *ep = &f->ep;
	unsigned result = 0;
	const struct darray *arrays[] = {&ep->params.da,
					 &ep->structs.da,
					 &ep->funcs.da,
					 &ep->samplers.da,
					 &ep->techniques.da,
					 &ep->cfp.lex.tokens.da,
					 &ep->cfp.pp.tokens.da,
					 &ep->cfp.pp.defines.da,
					 &ep->cfp.pp.dependencies.da,
					 &ep->cfp.pp.sys_include_dirs.da,
					 &ep->cfp.error_list.errors.da};
	for (size_t i = 0; i < sizeof(arrays) / sizeof(arrays[0]); i++)
		if (arrays[i]->array || arrays[i]->num || arrays[i]->capacity)
			result |= 1U;
	if (ep->cur_pass || ep->cfp.cur_token)
		result |= 2U;
	if (ep->effect != (gs_effect_t *)f->borrowed || strcmp(f->borrowed, "borrowed") != 0 ||
	    ep->files.array != f->files || ep->files.num != 1 || ep->tokens.array != f->tokens || ep->tokens.num != 1)
		result |= 4U;
	if (bnum_allocs() != f->before + 4)
		result |= 8U;
	da_free(ep->files);
	da_free(ep->tokens);
	bfree(f->borrowed);
	long before = f->before;
	bfree(f);
	if (bnum_allocs() != before)
		result |= 16U;
	return result;
}
