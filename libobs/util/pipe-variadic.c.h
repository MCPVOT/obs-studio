/*
 * Variadic half of util/pipe.c, compiled only when ENABLE_RUST_LIBOBS=ON.
 *
 * Stable Rust cannot define `os_process_args_add_argf` (c_variadic is
 * unstable), so this file formats the arguments with `dstr_vprintf` and
 * stores the result through `os_process_args_add_arg`.
 */

#include "build/libobs-util.pch.h"

void os_process_args_add_argf(struct os_process_args *args, const char *format, ...)
{
	/* dstr_vprintf, as util/pipe.c: a vsnprintf(NULL, 0, ...) of its own
	 * fails GCC's -fsanitize=undefined fortify check (#2005). */
	struct dstr buf = {0};
	va_list ap;
	va_start(ap, format);
	dstr_vprintf(&buf, format, ap);
	va_end(ap);

	os_process_args_add_arg(args, buf.array ? buf.array : "");
	dstr_free(&buf);
}
