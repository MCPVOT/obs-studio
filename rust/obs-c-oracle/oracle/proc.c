/* Test-only oracle: the original libobs/callback/proc.c.h, unmodified,
 * with its exports renamed to oracle_* so it can link next to the Rust
 * implementation. parse_decl_string, bmalloc, and blog stay the real
 * symbols: the Rust port and this oracle share them. */
#include <stddef.h>

#define proc_handler_create oracle_proc_handler_create
#define proc_handler_destroy oracle_proc_handler_destroy
#define proc_handler_add oracle_proc_handler_add
#define proc_handler_call oracle_proc_handler_call

/* libobs assigns without the sign-conversion warning the oracle enables. */
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
/* proc_handler_call copies info only when it is found and reads the copy
 * only then; MSVC cannot see that (C4701) and the oracle builds with /WX. */
#ifdef _MSC_VER
#pragma warning(disable : 4701)
#endif

#include "callback/proc.c.h"
