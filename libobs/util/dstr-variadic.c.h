/*
 * Copyright (c) 2023 Lain Bailey <lain@obsproject.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Variadic half of util/dstr-libc.c, compiled only in the hybrid libobs.
 *
 * Stable Rust cannot define C-variadic functions or name a `va_list`, so
 * these four stay C, copied verbatim from util/dstr-libc.c (which the C
 * reference still compiles whole). The dstr calls they make are the Rust
 * port's.
 */

#include "build/libobs-util.pch.h"

void dstr_printf(struct dstr *dst, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	dstr_vprintf(dst, format, args);
	va_end(args);
}

void dstr_catf(struct dstr *dst, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	dstr_vcatf(dst, format, args);
	va_end(args);
}

void dstr_vprintf(struct dstr *dst, const char *format, va_list args)
{
	va_list args_cp;
	va_copy(args_cp, args);

	int len = vsnprintf(NULL, 0, format, args_cp);
	va_end(args_cp);

	if (len < 0)
		len = 4095;

	dstr_ensure_capacity(dst, ((size_t)len) + 1);
	len = vsnprintf(dst->array, ((size_t)len) + 1, format, args);

	if (!*dst->array) {
		dstr_free(dst);
		return;
	}

	dst->len = len < 0 ? strlen(dst->array) : (size_t)len;
}

void dstr_vcatf(struct dstr *dst, const char *format, va_list args)
{
	va_list args_cp;
	va_copy(args_cp, args);

	int len = vsnprintf(NULL, 0, format, args_cp);
	va_end(args_cp);

	if (len < 0)
		len = 4095;

	dstr_ensure_capacity(dst, dst->len + ((size_t)len) + 1);
	len = vsnprintf(dst->array + dst->len, ((size_t)len) + 1, format, args);

	if (!*dst->array) {
		dstr_free(dst);
		return;
	}

	dst->len += len < 0 ? strlen(dst->array + dst->len) : (size_t)len;
}
