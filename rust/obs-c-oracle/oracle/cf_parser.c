/* Original parser compiled unchanged under private oracle names. */
#define cf_adderror oracle_cf_parser_adderror
#define cf_pass_pair oracle_cf_parser_pass_pair
#define lexer_getstroffset oracle_lexer_getstroffset
#define error_data_add oracle_error_data_add
#define dstr_safe_printf oracle_dstr_safe_printf
#include "util/cf-parser.c.h"

size_t oracle_cf_parser_layout(size_t index);
size_t oracle_cf_parser_layout(size_t index)
{
	const size_t layout[] = {
		sizeof(struct cf_parser),
		_Alignof(struct cf_parser),
		offsetof(struct cf_parser, lex),
		offsetof(struct cf_parser, pp),
		offsetof(struct cf_parser, error_list),
		offsetof(struct cf_parser, cur_token),
		sizeof(struct cf_preprocessor),
		_Alignof(struct cf_preprocessor),
		offsetof(struct cf_preprocessor, lex),
		offsetof(struct cf_preprocessor, ed),
		offsetof(struct cf_preprocessor, defines),
		offsetof(struct cf_preprocessor, sys_include_dirs),
		offsetof(struct cf_preprocessor, dependencies),
		offsetof(struct cf_preprocessor, tokens),
		offsetof(struct cf_preprocessor, ignore_state),
	};
	return layout[index];
}
