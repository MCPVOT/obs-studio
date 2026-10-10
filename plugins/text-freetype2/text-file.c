#include <stdio.h>
#include <string.h>

#include <util/bmem.h>
#include <util/platform.h>

#include "text-file.h"

static void remove_cr(wchar_t *source)
{
	int j = 0;
	for (int i = 0; source[i] != '\0'; ++i) {
		if (source[i] != L'\r') {
			source[j++] = source[i];
		}
	}
	source[j] = '\0';
}

wchar_t *ft2_read_text_file(const char *filename)
{
	wchar_t *text = NULL;
	FILE *tmp_file = NULL;
	uint32_t filesize = 0;
	char *tmp_read = NULL;
	uint16_t header = 0;
	size_t bytes_read;

	tmp_file = os_fopen(filename, "rb");
	if (tmp_file == NULL)
		return NULL;
	fseek(tmp_file, 0, SEEK_END);
	filesize = (uint32_t)ftell(tmp_file);
	fseek(tmp_file, 0, SEEK_SET);
	bytes_read = fread(&header, 1, 2, tmp_file);

	if (bytes_read == 2 && header == 0xFEFF) {
		// File is already in UTF-16 format
		text = bzalloc(filesize);
		bytes_read = fread(text, filesize - 2, 1, tmp_file);

		bfree(tmp_read);
		fclose(tmp_file);

		return text;
	}

	fseek(tmp_file, 0, SEEK_SET);

	tmp_read = bzalloc(filesize + 1);
	bytes_read = fread(tmp_read, filesize, 1, tmp_file);
	fclose(tmp_file);

	text = bzalloc((strlen(tmp_read) + 1) * sizeof(wchar_t));
	os_utf8_to_wcs(tmp_read, strlen(tmp_read), text, (strlen(tmp_read) + 1));

	remove_cr(text);
	bfree(tmp_read);
	return text;
}

wchar_t *ft2_read_text_file_end(const char *filename, uint32_t log_lines)
{
	wchar_t *text = NULL;
	FILE *tmp_file = NULL;
	uint32_t filesize = 0, cur_pos = 0;
	char *tmp_read = NULL;
	uint16_t value = 0, line_breaks = 0;
	size_t bytes_read;
	char bvalue;

	bool utf16 = false;

	tmp_file = fopen(filename, "rb");
	if (tmp_file == NULL)
		return NULL;
	bytes_read = fread(&value, 1, 2, tmp_file);

	if (bytes_read == 2 && value == 0xFEFF)
		utf16 = true;

	fseek(tmp_file, 0, SEEK_END);
	filesize = (uint32_t)ftell(tmp_file);
	cur_pos = filesize;

	while (line_breaks <= log_lines && cur_pos != 0) {
		if (!utf16)
			cur_pos--;
		else
			cur_pos -= 2;
		fseek(tmp_file, cur_pos, SEEK_SET);

		if (!utf16) {
			bytes_read = fread(&bvalue, 1, 1, tmp_file);
			if (bytes_read == 1 && bvalue == '\n')
				line_breaks++;
		} else {
			bytes_read = fread(&value, 1, 2, tmp_file);
			if (bytes_read == 2 && value == L'\n')
				line_breaks++;
		}
	}

	if (cur_pos != 0)
		cur_pos += (utf16) ? 2 : 1;

	fseek(tmp_file, cur_pos, SEEK_SET);

	if (utf16) {
		text = bzalloc(filesize - cur_pos);
		bytes_read = fread(text, (filesize - cur_pos), 1, tmp_file);

		remove_cr(text);
		bfree(tmp_read);
		fclose(tmp_file);

		return text;
	}

	tmp_read = bzalloc((filesize - cur_pos) + 1);
	bytes_read = fread(tmp_read, filesize - cur_pos, 1, tmp_file);
	fclose(tmp_file);

	text = bzalloc((strlen(tmp_read) + 1) * sizeof(wchar_t));
	os_utf8_to_wcs(tmp_read, strlen(tmp_read), text, (strlen(tmp_read) + 1));

	remove_cr(text);
	bfree(tmp_read);
	return text;
}
