#pragma once

/* Private (#874): opens profiler-io.c.h's csv.gz for writing; the host files
 * profiler-io-host-<os>.c.h implement it. NULL on failure. */

#include <zlib.h>

gzFile profiler_gzopen(const char *filename);
