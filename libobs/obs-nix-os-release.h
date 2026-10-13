#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "util/dstr.h"

static inline void read_distribution_info(FILE *fp, struct dstr *distro, struct dstr *version)
{
	char *line = NULL;
	size_t linecap = 0;
	dstr_init_copy(distro, "Unknown");
	dstr_init_copy(version, "Unknown");
	while (getline(&line, &linecap, fp) != -1) {
		if (!strncmp(line, "NAME", 4)) {
			char *start = strchr(line, '=');
			if (!start || *(++start) == '\0')
				continue;
			dstr_copy(distro, start);
			dstr_resize(distro, distro->len - 1);
		}
		if (!strncmp(line, "VERSION_ID", 10)) {
			char *start = strchr(line, '=');
			if (!start || *(++start) == '\0')
				continue;
			dstr_copy(version, start);
			dstr_resize(version, version->len - 1);
		}
	}
	free(line);
}
