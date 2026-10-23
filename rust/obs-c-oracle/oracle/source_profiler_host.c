/* Test-only stand-ins for the libobs core calls the Rust source-profiler
 * shim (rust/obs-core/src/ffi/source_profiler.rs) makes. libobs supplies the
 * real ones; cargo test links no libobs core, so these let the obs-core shim
 * link and run with no graphics: no context, NULL timers and ranges, no
 * async flag. The graphics task queue runs the task at once. The C oracle of
 * source-profiler.c (oracle/source_profiler.c) uses its own renamed
 * stand-ins, not these. */

#include "obs-internal.h"

void obs_enter_graphics(void) {}

void obs_leave_graphics(void) {}

void obs_queue_task(enum obs_task_type type, obs_task_t task, void *param, bool wait)
{
	UNUSED_PARAMETER(type);
	UNUSED_PARAMETER(wait);
	task(param);
}

uint32_t obs_source_get_output_flags(const obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	return 0;
}

uint64_t obs_source_get_last_async_ts(const obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	return 0;
}

gs_timer_t *gs_timer_create(void)
{
	return NULL;
}

void gs_timer_destroy(gs_timer_t *timer)
{
	UNUSED_PARAMETER(timer);
}

void gs_timer_begin(gs_timer_t *timer)
{
	UNUSED_PARAMETER(timer);
}

void gs_timer_end(gs_timer_t *timer)
{
	UNUSED_PARAMETER(timer);
}

bool gs_timer_get_data(gs_timer_t *timer, uint64_t *ticks)
{
	UNUSED_PARAMETER(timer);
	UNUSED_PARAMETER(ticks);
	return false;
}

gs_timer_range_t *gs_timer_range_create(void)
{
	return NULL;
}

void gs_timer_range_destroy(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

void gs_timer_range_begin(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

void gs_timer_range_end(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

bool gs_timer_range_get_data(gs_timer_range_t *range, bool *disjoint, uint64_t *frequency)
{
	UNUSED_PARAMETER(range);
	UNUSED_PARAMETER(disjoint);
	UNUSED_PARAMETER(frequency);
	return false;
}
