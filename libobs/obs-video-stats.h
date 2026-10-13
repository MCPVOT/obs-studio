#pragma once

#include <pthread.h>
#include <stdint.h>

/* Private graphics-thread statistics read by public obs_get_* accessors. */
struct obs_video_stats {
	pthread_mutex_t mutex;
	uint64_t avg_frame_time_ns;
	double fps;
	uint32_t total_frames;
	uint32_t lagged_frames;
};

static inline int obs_video_stats_init(struct obs_video_stats *stats)
{
	return pthread_mutex_init(&stats->mutex, NULL);
}

static inline void obs_video_stats_destroy(struct obs_video_stats *stats)
{
	pthread_mutex_destroy(&stats->mutex);
}

static inline void obs_video_stats_add_frames(struct obs_video_stats *stats, uint32_t total, uint32_t lagged)
{
	stats->total_frames += total;
	stats->lagged_frames += lagged;
}

static inline void obs_video_stats_set_timing(struct obs_video_stats *stats, double fps, uint64_t frame_time_ns)
{
	stats->fps = fps;
	stats->avg_frame_time_ns = frame_time_ns;
}

static inline uint32_t obs_video_stats_total_frames(struct obs_video_stats *stats)
{
	return stats->total_frames;
}

static inline uint32_t obs_video_stats_lagged_frames(struct obs_video_stats *stats)
{
	return stats->lagged_frames;
}

static inline double obs_video_stats_fps(struct obs_video_stats *stats)
{
	return stats->fps;
}

static inline uint64_t obs_video_stats_avg_frame_time_ns(struct obs_video_stats *stats)
{
	return stats->avg_frame_time_ns;
}
