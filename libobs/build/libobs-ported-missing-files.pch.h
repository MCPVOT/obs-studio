/* Precompiled header (BUILD.md) for the libobs unity group
 * libobs.ported-missing-files. obs-missing-files.c alone, without the core,
 * so the C oracle and C tests can compile it. The implementation file
 * includes this header and nothing else. */

#pragma once

#include <string.h>

#include "util/bmem.h"
#include "util/darray.h"
#include "util/threading.h"
#include "obs-missing-files.h"

struct obs_source;
typedef struct obs_source obs_source_t;

/* Stay in obs-source.c.h. Declared here so this file compiles without obs.h. */
const char *obs_source_get_name(const obs_source_t *source);
void obs_source_replace_missing_file(obs_missing_file_cb cb, obs_source_t *source, const char *new_path, void *data);
