/* Verbatim original C unit and header-derived layouts. No behavior is
 * reimplemented by the oracle. Numeric parsing uses this target's CRT. */
#include <errno.h>
#include <stddef.h>

#define shader_sampler_convert oracle_shader_sampler_convert
#define get_sample_filter oracle_get_sample_filter
#define get_address_mode oracle_get_address_mode
#define astrcmpi oracle_astrcmpi
#include "graphics/shader-sampler-convert.c.h"

size_t oracle_shader_sampler_layout(size_t index);
int oracle_shader_sampler_errno(void (*convert)(void *, void *), void *sampler, void *info);

size_t oracle_shader_sampler_layout(size_t index)
{
	const size_t values[] = {
		sizeof(struct shader_sampler),
		_Alignof(struct shader_sampler),
		offsetof(struct shader_sampler, name),
		offsetof(struct shader_sampler, states),
		offsetof(struct shader_sampler, values),
		sizeof(struct gs_sampler_info),
		_Alignof(struct gs_sampler_info),
		offsetof(struct gs_sampler_info, filter),
		offsetof(struct gs_sampler_info, address_u),
		offsetof(struct gs_sampler_info, address_v),
		offsetof(struct gs_sampler_info, address_w),
		offsetof(struct gs_sampler_info, max_anisotropy),
		offsetof(struct gs_sampler_info, border_color),
	};
	return values[index];
}

int oracle_shader_sampler_errno(void (*convert)(void *, void *), void *sampler, void *info)
{
	errno = E2BIG;
	convert(sampler, info);
	return errno;
}
