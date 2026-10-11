#include "output-filename-format.h"

#include <stdlib.h>
#include <string.h>

char *output_filename_format(const char *format, const char *prefix, const char *suffix)
{
	(void)prefix;
	(void)suffix;

	if (!format)
		format = "";

	size_t len = strlen(format);
	char *out = malloc(len + 1);

	if (out)
		memcpy(out, format, len + 1);
	return out;
}
