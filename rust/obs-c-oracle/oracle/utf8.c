/* Test-only oracle: the original libobs/util/utf8.c, unmodified, with its
 * global symbols renamed to oracle_* so they can link next to the Rust
 * implementation. */
#define utf8_to_wchar oracle_utf8_to_wchar
#define wchar_to_utf8 oracle_wchar_to_utf8

#include "util/utf8.c.h"
/* Windows keeps its converter in its own unity group member (#876). */
#ifdef UTF8_HOST_WINDOWS
#include "util/utf8-host-windows.c.h"
#endif
