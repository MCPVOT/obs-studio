/* Test-only oracle: the original libobs/callback/calldata.c.h, unmodified,
 * with its exports renamed to oracle_* so it can link next to the Rust
 * implementation. bmalloc and blog stay the real symbols: the Rust port
 * and this oracle share them. */
#include <stddef.h>

#define calldata_get_data oracle_calldata_get_data
#define calldata_set_data oracle_calldata_set_data
#define calldata_get_string oracle_calldata_get_string

/* Pointer subtraction is a ptrdiff_t. libobs assigns it to size_t.
 * The oracle builds with -Wsign-conversion and libobs does not. */
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "callback/calldata.c.h"

size_t oracle_sizeof_calldata(void);
size_t oracle_alignof_calldata(void);
size_t oracle_offsetof_calldata_stack(void);
size_t oracle_offsetof_calldata_size(void);
size_t oracle_offsetof_calldata_capacity(void);
size_t oracle_offsetof_calldata_fixed(void);

size_t oracle_sizeof_calldata(void)
{
	return sizeof(struct calldata);
}

size_t oracle_alignof_calldata(void)
{
	return _Alignof(struct calldata);
}

size_t oracle_offsetof_calldata_stack(void)
{
	return offsetof(struct calldata, stack);
}

size_t oracle_offsetof_calldata_size(void)
{
	return offsetof(struct calldata, size);
}

size_t oracle_offsetof_calldata_capacity(void)
{
	return offsetof(struct calldata, capacity);
}

size_t oracle_offsetof_calldata_fixed(void)
{
	return offsetof(struct calldata, fixed);
}
