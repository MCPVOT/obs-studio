#pragma once

#include <stdint.h>
#include <wchar.h>

/* The text of a UTF-8 or UTF-16 (with BOM) file, without carriage returns,
 * as a bmalloc'd wide string; NULL if the file cannot be opened. */
wchar_t *ft2_read_text_file(const char *filename);

/* As ft2_read_text_file, but only the last log_lines lines. */
wchar_t *ft2_read_text_file_end(const char *filename, uint32_t log_lines);
