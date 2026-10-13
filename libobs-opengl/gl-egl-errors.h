#pragma once

#include <stdlib.h>
#include <glad/glad_egl.h>
#include <util/base.h>

static inline void gl_egl_log_init_error(const char *operation)
{
	blog(LOG_ERROR, "%s failed", operation);
}
