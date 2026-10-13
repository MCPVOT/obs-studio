#include "../../libobs/obs-video-stats.h"

#include <assert.h>

static struct obs_video_stats stats;

static void *publish_stats(void *unused)
{
	(void)unused;
	for (uint32_t i = 0; i < 100000; i++) {
		obs_video_stats_add_frames(&stats, 1, 1);
		obs_video_stats_set_timing(&stats, 60.0, 16666667);
	}
	return NULL;
}

static void *read_stats(void *unused)
{
	(void)unused;
	for (uint32_t i = 0; i < 100000; i++) {
		assert(obs_video_stats_total_frames(&stats) <= 100000);
		assert(obs_video_stats_lagged_frames(&stats) <= 100000);
		assert(obs_video_stats_fps(&stats) <= 60.0);
		assert(obs_video_stats_avg_frame_time_ns(&stats) <= 16666667);
	}
	return NULL;
}

int main(void)
{
	assert(obs_video_stats_init(&stats) == 0);
	pthread_t writer;
	pthread_t reader;
	assert(pthread_create(&writer, NULL, publish_stats, NULL) == 0);
	assert(pthread_create(&reader, NULL, read_stats, NULL) == 0);
	assert(pthread_join(writer, NULL) == 0);
	assert(pthread_join(reader, NULL) == 0);
	obs_video_stats_destroy(&stats);
}
