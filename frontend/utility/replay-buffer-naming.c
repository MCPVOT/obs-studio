#include "replay-buffer-naming.h"
#include "output-filename-format.h"

#include <stdlib.h>

void replay_buffer_naming_set(obs_data_t *settings, const struct replay_buffer_naming *naming)
{
	char *format = output_filename_format(naming->filename_format, naming->prefix, naming->suffix);

	obs_data_set_string(settings, "directory", naming->directory ? naming->directory : "");
	obs_data_set_string(settings, "format", format ? format : "");
	obs_data_set_string(settings, "extension", naming->extension ? naming->extension : "");
	obs_data_set_bool(settings, "allow_spaces", naming->allow_spaces);

	free(format);
}

void replay_buffer_naming_refresh(obs_output_t *replay_buffer, const struct replay_buffer_naming *naming)
{
	(void)replay_buffer;
	(void)naming;
}
