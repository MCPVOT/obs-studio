/* Test-only oracle: the original libobs/util/source-profiler.c.h,
 * unmodified, with its non-static names renamed to oracle_* so it links next
 * to the Rust shims, and everything it asks libobs for renamed to
 * deterministic stand-ins defined below: the clock (a queue the test
 * fills), GPU timers and ranges (numbered handles; a timer reports the ticks
 * set when it was created), the graphics context (a depth counter;
 * obs->video.graphics is a zeroed core), the graphics task queue (runs the
 * task at once), the async timestamp, the log line and the profiler scope.
 * Sources are zeroed struct obs_source blocks with info.output_flags set.
 * The test drives the same operations through the Rust core with a host
 * that answers alike, so whole sequences compare (Tier 3).
 *
 * bmalloc, bzalloc, bfree, the darray and pthread rwlock calls keep their
 * real names (obs-util's Rust ports and the C runtime under cargo test).
 * The real-name stand-ins the Rust shim links against under cargo test are
 * in oracle/source_profiler_host.c. */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define source_profiler_enable oracle_source_profiler_enable
#define source_profiler_gpu_enable oracle_source_profiler_gpu_enable
#define source_profiler_get_result oracle_source_profiler_get_result
#define source_profiler_fill_result oracle_source_profiler_fill_result
#define source_profiler_reset_video oracle_source_profiler_reset_video
#define source_profiler_render_begin oracle_source_profiler_render_begin
#define source_profiler_render_end oracle_source_profiler_render_end
#define source_profiler_frame_begin oracle_source_profiler_frame_begin
#define source_profiler_frame_collect oracle_source_profiler_frame_collect
#define source_profiler_async_frame_received oracle_source_profiler_async_frame_received
#define source_profiler_source_tick_start oracle_source_profiler_source_tick_start
#define source_profiler_source_tick_end oracle_source_profiler_source_tick_end
#define source_profiler_source_render_begin oracle_source_profiler_source_render_begin
#define source_profiler_source_render_end oracle_source_profiler_source_render_end
#define source_profiler_remove_source oracle_source_profiler_remove_source
#define ucirclebuf_init oracle_ucirclebuf_init
#define ucirclebuf_free oracle_ucirclebuf_free
#define ucirclebuf_push oracle_ucirclebuf_push
#define source_samples_create oracle_source_samples_create
#define hm_samples oracle_sp_hm_samples
#define hm_entries oracle_sp_hm_entries
#define hm_rwlock oracle_sp_hm_rwlock

#define os_gettime_ns oracle_sp_gettime_ns
#define gs_enter_context oracle_sp_enter_context
#define gs_leave_context oracle_sp_leave_context
#define gs_timer_create oracle_sp_timer_create
#define gs_timer_destroy oracle_sp_timer_destroy
#define gs_timer_begin oracle_sp_timer_begin
#define gs_timer_end oracle_sp_timer_end
#define gs_timer_get_data oracle_sp_timer_get_data
#define gs_timer_range_create oracle_sp_range_create
#define gs_timer_range_destroy oracle_sp_range_destroy
#define gs_timer_range_begin oracle_sp_range_begin
#define gs_timer_range_end oracle_sp_range_end
#define gs_timer_range_get_data oracle_sp_range_get_data
#define obs_queue_task oracle_sp_queue_task
#define obs_source_get_last_async_ts oracle_sp_last_async_ts
#define profile_start oracle_sp_profile_start
#define profile_end oracle_sp_profile_end
#define blog oracle_sp_blog
#define obs oracle_sp_obs

/* The C file defines these without a prototype (they are non-static but
 * file-local); -Wmissing-prototypes wants one. */
struct ucirclebuf;
struct source_samples;
void oracle_ucirclebuf_init(struct ucirclebuf *buf, size_t capacity);
void oracle_ucirclebuf_free(struct ucirclebuf *buf);
void oracle_ucirclebuf_push(struct ucirclebuf *buf, uint64_t val);
struct source_samples *oracle_source_samples_create(const uintptr_t key);

#include "util/source-profiler.c.h"

/* Prototypes for -Wmissing-prototypes; the stand-ins replace declarations
 * the headers made under the original names. */
uint64_t oracle_sp_gettime_ns(void);
void oracle_sp_enter_context(graphics_t *graphics);
void oracle_sp_leave_context(void);
gs_timer_t *oracle_sp_timer_create(void);
void oracle_sp_timer_destroy(gs_timer_t *timer);
void oracle_sp_timer_begin(gs_timer_t *timer);
void oracle_sp_timer_end(gs_timer_t *timer);
bool oracle_sp_timer_get_data(gs_timer_t *timer, uint64_t *ticks);
gs_timer_range_t *oracle_sp_range_create(void);
void oracle_sp_range_destroy(gs_timer_range_t *range);
void oracle_sp_range_begin(gs_timer_range_t *range);
void oracle_sp_range_end(gs_timer_range_t *range);
bool oracle_sp_range_get_data(gs_timer_range_t *range, bool *disjoint, uint64_t *frequency);
void oracle_sp_queue_task(enum obs_task_type type, obs_task_t task, void *param, bool wait);
uint64_t oracle_sp_last_async_ts(const obs_source_t *source);
void oracle_sp_profile_start(const char *name);
void oracle_sp_profile_end(const char *name);
void oracle_sp_blog(int log_level, const char *format, ...);
void oracle_sp_reset(void);
void oracle_sp_set_clock_step(uint64_t step);
void oracle_sp_push_time(uint64_t t);
size_t oracle_sp_pending_times(void);
void oracle_sp_set_next_ticks(bool ok, uint64_t ticks);
void oracle_sp_set_range(bool disjoint, uint64_t frequency);
void oracle_sp_set_null_handles(bool null_handles);
long oracle_sp_context_depth(void);
size_t oracle_sp_warnings(void);
size_t oracle_sp_take_destroyed_timers(uintptr_t *out, size_t cap);
size_t oracle_sp_take_destroyed_ranges(uintptr_t *out, size_t cap);
obs_source_t *oracle_sp_source_new(uint32_t output_flags);
void oracle_sp_source_set_last_async_ts(obs_source_t *source, uint64_t ts);
void oracle_sp_source_free(obs_source_t *source);
void oracle_sp_state(bool *enabled, bool *gpu, uint8_t *idx, uintptr_t ranges[NUM_TEXTURES], uint64_t *samples);
size_t oracle_sp_frame_buffer_size(void);
bool oracle_sp_sample(const obs_source_t *source, size_t frame, uint8_t *frame_idx, uint64_t *tick, uint64_t *cpu,
		      size_t cpu_cap, size_t *cpu_num, uintptr_t *timers, size_t timers_cap, size_t *timers_num);
size_t oracle_sp_samples_count(void);
size_t oracle_sp_entries_count(void);
bool oracle_sp_entry_ring(const obs_source_t *source, int which, uint64_t *out, size_t cap, size_t *num, size_t *idx,
			  size_t *capacity);
void oracle_sp_ucirclebuf_run(size_t capacity, const uint64_t *values, size_t count, uint64_t *out, size_t *num,
			      size_t *idx);
void oracle_sp_calculate(uint64_t *tick, size_t tick_num, uint64_t *cpu, uint64_t *cpu_sum, size_t cpu_num,
			 uint64_t *gpu, uint64_t *gpu_sum, size_t gpu_num, bool gpu_on, uint64_t *frames,
			 size_t frames_num, struct profiler_result *result);
size_t oracle_sp_result_size(void);
size_t oracle_sp_result_align(void);
size_t oracle_sp_result_offset(int field);
size_t oracle_sp_ovi_fps_num_offset(void);
size_t oracle_sp_ovi_fps_den_offset(void);

/* source-profiler.c.h undefines FRAME_BUFFER_SIZE at its end; it is
 * NUM_TEXTURES. */

/* ------------------------------------------------------------------------- */
/* stand-ins */

#define SP_MAX_HANDLES 65536
#define SP_MAX_TIMES 65536
#define SP_MAX_LOG 65536

static struct obs_core sp_core;
struct obs_core *oracle_sp_obs = &sp_core;

static uint64_t sp_times[SP_MAX_TIMES];
/* With no scripted reading queued: a clock advancing by sp_clock_step (0
 * unless set, for the benchmark). */
static uint64_t sp_clock;
static uint64_t sp_clock_step;
static size_t sp_times_head;
static size_t sp_times_tail;

static uintptr_t sp_next_handle;
static bool sp_null_handles;
static bool sp_next_ticks_ok;
static uint64_t sp_next_ticks;
static bool sp_ticks_ok[SP_MAX_HANDLES];
static uint64_t sp_ticks[SP_MAX_HANDLES];
static bool sp_range_disjoint;
static uint64_t sp_range_frequency;

static long sp_context_depth;
static long sp_context_max_depth;
static size_t sp_warnings;
static uintptr_t sp_destroyed_timers[SP_MAX_LOG];
static size_t sp_destroyed_timers_num;
static uintptr_t sp_destroyed_ranges[SP_MAX_LOG];
static size_t sp_destroyed_ranges_num;
static uintptr_t sp_created_ranges[SP_MAX_LOG];
static size_t sp_created_ranges_num;

#define SP_HANDLE_BASE ((uintptr_t)0x9000)
#define SP_HANDLE_STEP ((uintptr_t)0x10)

uint64_t oracle_sp_gettime_ns(void)
{
	if (sp_times_head == sp_times_tail)
		return sp_clock += sp_clock_step;
	return sp_times[sp_times_head++ % SP_MAX_TIMES];
}

void oracle_sp_enter_context(graphics_t *graphics)
{
	UNUSED_PARAMETER(graphics);
	if (++sp_context_depth > sp_context_max_depth)
		sp_context_max_depth = sp_context_depth;
}

void oracle_sp_leave_context(void)
{
	sp_context_depth--;
}

static uintptr_t sp_handle(void)
{
	if (sp_null_handles)
		return 0;
	uintptr_t h = sp_next_handle;
	sp_next_handle += SP_HANDLE_STEP;
	return h;
}

static size_t sp_slot(uintptr_t h)
{
	return (size_t)((h - SP_HANDLE_BASE) / SP_HANDLE_STEP) % SP_MAX_HANDLES;
}

gs_timer_t *oracle_sp_timer_create(void)
{
	uintptr_t h = sp_handle();
	if (h) {
		sp_ticks_ok[sp_slot(h)] = sp_next_ticks_ok;
		sp_ticks[sp_slot(h)] = sp_next_ticks;
	}
	return (gs_timer_t *)h;
}

void oracle_sp_timer_destroy(gs_timer_t *timer)
{
	if (sp_destroyed_timers_num < SP_MAX_LOG)
		sp_destroyed_timers[sp_destroyed_timers_num++] = (uintptr_t)timer;
}

void oracle_sp_timer_begin(gs_timer_t *timer)
{
	UNUSED_PARAMETER(timer);
}

void oracle_sp_timer_end(gs_timer_t *timer)
{
	UNUSED_PARAMETER(timer);
}

bool oracle_sp_timer_get_data(gs_timer_t *timer, uint64_t *ticks)
{
	uintptr_t h = (uintptr_t)timer;
	if (!h || !sp_ticks_ok[sp_slot(h)])
		return false;
	*ticks = sp_ticks[sp_slot(h)];
	return true;
}

gs_timer_range_t *oracle_sp_range_create(void)
{
	uintptr_t h = sp_handle();
	if (sp_created_ranges_num < SP_MAX_LOG)
		sp_created_ranges[sp_created_ranges_num++] = h;
	return (gs_timer_range_t *)h;
}

void oracle_sp_range_destroy(gs_timer_range_t *range)
{
	if (sp_destroyed_ranges_num < SP_MAX_LOG)
		sp_destroyed_ranges[sp_destroyed_ranges_num++] = (uintptr_t)range;
}

void oracle_sp_range_begin(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

void oracle_sp_range_end(gs_timer_range_t *range)
{
	UNUSED_PARAMETER(range);
}

bool oracle_sp_range_get_data(gs_timer_range_t *range, bool *disjoint, uint64_t *frequency)
{
	UNUSED_PARAMETER(range);
	*disjoint = sp_range_disjoint;
	*frequency = sp_range_frequency;
	return true;
}

/* The graphics thread runs the task at its next execute_graphics_tasks;
 * the test's operations all stand for the graphics thread, so at once. */
void oracle_sp_queue_task(enum obs_task_type type, obs_task_t task, void *param, bool wait)
{
	UNUSED_PARAMETER(type);
	UNUSED_PARAMETER(wait);
	task(param);
}

uint64_t oracle_sp_last_async_ts(const obs_source_t *source)
{
	return source->async_last_rendered_ts;
}

void oracle_sp_profile_start(const char *name)
{
	UNUSED_PARAMETER(name);
}

void oracle_sp_profile_end(const char *name)
{
	UNUSED_PARAMETER(name);
}

void oracle_sp_blog(int log_level, const char *format, ...)
{
	if (log_level == LOG_WARNING && strcmp(format, "GPU Timers were disjoint, discarding samples.") == 0)
		sp_warnings++;
}

/* ------------------------------------------------------------------------- */
/* test controls */

/* Everything back to the start of the process: no samples, no entries, no
 * timer ranges, every flag off, and the stand-ins cleared. */
void oracle_sp_reset(void)
{
	profiler_shutdown();
	sp_enabled = false;
	gpu_enabled = false;
	enable_next = false;
	gpu_enable_next = false;
	timer_idx = 0;
	profiler_samples = 0;

	sp_times_head = sp_times_tail = 0;
	sp_clock = sp_clock_step = 0;
	sp_next_handle = SP_HANDLE_BASE;
	sp_null_handles = false;
	sp_next_ticks_ok = true;
	sp_next_ticks = 0;
	memset(sp_ticks_ok, 0, sizeof(sp_ticks_ok));
	sp_range_disjoint = false;
	sp_range_frequency = 1000000000ULL;
	sp_context_depth = sp_context_max_depth = 0;
	sp_warnings = 0;
	sp_destroyed_timers_num = 0;
	sp_destroyed_ranges_num = 0;
	sp_created_ranges_num = 0;
}

void oracle_sp_set_clock_step(uint64_t step)
{
	sp_clock_step = step;
}

void oracle_sp_push_time(uint64_t t)
{
	sp_times[sp_times_tail++ % SP_MAX_TIMES] = t;
}

size_t oracle_sp_pending_times(void)
{
	return sp_times_tail - sp_times_head;
}

void oracle_sp_set_next_ticks(bool ok, uint64_t ticks)
{
	sp_next_ticks_ok = ok;
	sp_next_ticks = ticks;
}

void oracle_sp_set_range(bool disjoint, uint64_t frequency)
{
	sp_range_disjoint = disjoint;
	sp_range_frequency = frequency;
}

void oracle_sp_set_null_handles(bool null_handles)
{
	sp_null_handles = null_handles;
}

long oracle_sp_context_depth(void)
{
	return sp_context_depth;
}

size_t oracle_sp_warnings(void)
{
	return sp_warnings;
}

size_t oracle_sp_take_destroyed_timers(uintptr_t *out, size_t cap)
{
	size_t n = sp_destroyed_timers_num < cap ? sp_destroyed_timers_num : cap;
	memcpy(out, sp_destroyed_timers, n * sizeof(*out));
	sp_destroyed_timers_num = 0;
	return n;
}

size_t oracle_sp_take_destroyed_ranges(uintptr_t *out, size_t cap)
{
	size_t n = sp_destroyed_ranges_num < cap ? sp_destroyed_ranges_num : cap;
	memcpy(out, sp_destroyed_ranges, n * sizeof(*out));
	sp_destroyed_ranges_num = 0;
	return n;
}

obs_source_t *oracle_sp_source_new(uint32_t output_flags)
{
	obs_source_t *source = bzalloc(sizeof(struct obs_source));
	source->info.output_flags = output_flags;
	return source;
}

void oracle_sp_source_set_last_async_ts(obs_source_t *source, uint64_t ts)
{
	source->async_last_rendered_ts = ts;
}

void oracle_sp_source_free(obs_source_t *source)
{
	bfree(source);
}

/* ------------------------------------------------------------------------- */
/* state */

void oracle_sp_state(bool *enabled, bool *gpu, uint8_t *idx, uintptr_t ranges[NUM_TEXTURES], uint64_t *samples)
{
	*enabled = sp_enabled;
	*gpu = gpu_enabled;
	*idx = timer_idx;
	for (size_t i = 0; i < NUM_TEXTURES; i++)
		ranges[i] = (uintptr_t)timer_ranges[i];
	*samples = profiler_samples;
}

size_t oracle_sp_frame_buffer_size(void)
{
	return NUM_TEXTURES;
}

/* A source's samples: false when it has none. frame selects the frame. */
bool oracle_sp_sample(const obs_source_t *source, size_t frame, uint8_t *frame_idx, uint64_t *tick, uint64_t *cpu,
		      size_t cpu_cap, size_t *cpu_num, uintptr_t *timers, size_t timers_cap, size_t *timers_num)
{
	struct source_samples *smp = NULL;
	HASH_FIND_PTR(hm_samples, &source, smp);
	if (!smp)
		return false;
	struct frame_sample *f = smp->frames[frame % NUM_TEXTURES];
	*frame_idx = smp->frame_idx;
	*tick = f->tick;
	*cpu_num = f->render_cpu.num;
	for (size_t i = 0; i < f->render_cpu.num && i < cpu_cap; i++)
		cpu[i] = f->render_cpu.array[i];
	*timers_num = f->render_timers.num;
	for (size_t i = 0; i < f->render_timers.num && i < timers_cap; i++)
		timers[i] = (uintptr_t)f->render_timers.array[i];
	return true;
}

size_t oracle_sp_samples_count(void)
{
	return HASH_COUNT(hm_samples);
}

size_t oracle_sp_entries_count(void)
{
	return HASH_COUNT(hm_entries);
}

static const struct ucirclebuf *sp_ring(const struct profiler_entry *ent, int which)
{
	switch (which) {
	case 0:
		return &ent->tick;
	case 1:
		return &ent->render_cpu;
	case 2:
		return &ent->render_gpu;
	case 3:
		return &ent->render_cpu_sum;
	case 4:
		return &ent->render_gpu_sum;
	case 5:
		return &ent->async_frame_ts;
	default:
		return &ent->async_rendered_ts;
	}
}

/* One ring of a source's entry (0 tick, 1 render_cpu, 2 render_gpu,
 * 3 render_cpu_sum, 4 render_gpu_sum, 5 async_frame_ts, 6 async_rendered_ts):
 * false when it has no entry. */
bool oracle_sp_entry_ring(const obs_source_t *source, int which, uint64_t *out, size_t cap, size_t *num, size_t *idx,
			  size_t *capacity)
{
	struct profiler_entry *ent = NULL;
	HASH_FIND_PTR(hm_entries, &source, ent);
	if (!ent)
		return false;
	const struct ucirclebuf *buf = sp_ring(ent, which);
	*num = buf->num;
	*idx = buf->idx;
	*capacity = buf->capacity;
	for (size_t i = 0; i < buf->num && i < cap; i++)
		out[i] = buf->array[i];
	return true;
}

/* ucirclebuf on its own: init with capacity, push values, report. */
void oracle_sp_ucirclebuf_run(size_t capacity, const uint64_t *values, size_t count, uint64_t *out, size_t *num,
			      size_t *idx)
{
	struct ucirclebuf buf = {0};
	oracle_ucirclebuf_init(&buf, capacity);
	for (size_t i = 0; i < count; i++)
		oracle_ucirclebuf_push(&buf, values[i]);
	*num = buf.num;
	*idx = buf.idx;
	for (size_t i = 0; i < buf.num; i++)
		out[i] = buf.array[i];
	oracle_ucirclebuf_free(&buf);
}

/* calculate_tick / calculate_render / calculate_fps on hand-filled rings.
 * The rings point at the caller's arrays, which are read only, never pushed
 * or freed (non-const because struct ucirclebuf's array is). */
static struct ucirclebuf sp_view(uint64_t *values, size_t num)
{
	struct ucirclebuf buf = {.idx = num, .capacity = num, .num = num, .array = values};
	return buf;
}

void oracle_sp_calculate(uint64_t *tick, size_t tick_num, uint64_t *cpu, uint64_t *cpu_sum, size_t cpu_num,
			 uint64_t *gpu, uint64_t *gpu_sum, size_t gpu_num, bool gpu_on, uint64_t *frames,
			 size_t frames_num, struct profiler_result *result)
{
	struct profiler_entry ent;
	memset(&ent, 0, sizeof(ent));
	ent.tick = sp_view(tick, tick_num);
	ent.render_cpu = sp_view(cpu, cpu_num);
	ent.render_cpu_sum = sp_view(cpu_sum, cpu_num);
	ent.render_gpu = sp_view(gpu, gpu_num);
	ent.render_gpu_sum = sp_view(gpu_sum, gpu_num);
	struct ucirclebuf fps = sp_view(frames, frames_num);

	bool saved = gpu_enabled;
	gpu_enabled = gpu_on;
	calculate_tick(&ent, result);
	calculate_render(&ent, result);
	gpu_enabled = saved;
	calculate_fps(&fps, &result->async_input, &result->async_input_best, &result->async_input_worst);
}

/* profiler_result_t layout. */
size_t oracle_sp_result_size(void)
{
	return sizeof(profiler_result_t);
}

size_t oracle_sp_result_align(void)
{
	return _Alignof(profiler_result_t);
}

size_t oracle_sp_result_offset(int field)
{
	switch (field) {
	case 0:
		return offsetof(profiler_result_t, tick_avg);
	case 1:
		return offsetof(profiler_result_t, tick_max);
	case 2:
		return offsetof(profiler_result_t, render_avg);
	case 3:
		return offsetof(profiler_result_t, render_max);
	case 4:
		return offsetof(profiler_result_t, render_gpu_avg);
	case 5:
		return offsetof(profiler_result_t, render_gpu_max);
	case 6:
		return offsetof(profiler_result_t, render_sum);
	case 7:
		return offsetof(profiler_result_t, render_gpu_sum);
	case 8:
		return offsetof(profiler_result_t, async_input);
	case 9:
		return offsetof(profiler_result_t, async_rendered);
	case 10:
		return offsetof(profiler_result_t, async_input_best);
	case 11:
		return offsetof(profiler_result_t, async_input_worst);
	case 12:
		return offsetof(profiler_result_t, async_rendered_best);
	default:
		return offsetof(profiler_result_t, async_rendered_worst);
	}
}

/* The head of struct obs_video_info the Rust shim reads. */
size_t oracle_sp_ovi_fps_num_offset(void)
{
	return offsetof(struct obs_video_info, fps_num);
}

size_t oracle_sp_ovi_fps_den_offset(void)
{
	return offsetof(struct obs_video_info, fps_den);
}
