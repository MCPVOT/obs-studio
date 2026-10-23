/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "libobs/build/libobs.pch.h"

/* struct obs_view's side of the views (#1933). The view functions moved to
 * obs-view-api.c, which the Rust port (obs-core's view module) replaces;
 * struct obs_view stays here, embedded in struct obs_canvas, and these
 * hidden accessors (declared in obs-internal.h) are all that the C file and
 * the Rust shims see of it, of a source's removed flag and of the video
 * mixes list. Each does one step on the struct and nothing else. */

size_t obs_view_struct_size(void)
{
	return sizeof(struct obs_view);
}

bool obs_view_mutex_init(struct obs_view *view)
{
	pthread_mutex_init_value(&view->channels_mutex);
	return pthread_mutex_init(&view->channels_mutex, NULL) == 0;
}

void obs_view_mutex_destroy(struct obs_view *view)
{
	pthread_mutex_destroy(&view->channels_mutex);
}

void obs_view_lock(struct obs_view *view)
{
	pthread_mutex_lock(&view->channels_mutex);
}

void obs_view_unlock(struct obs_view *view)
{
	pthread_mutex_unlock(&view->channels_mutex);
}

enum view_type obs_view_get_type(const struct obs_view *view)
{
	return view->type;
}

void obs_view_set_type(struct obs_view *view, enum view_type type)
{
	view->type = type;
}

obs_source_t *obs_view_channel(const struct obs_view *view, size_t channel)
{
	return view->channels[channel];
}

void obs_view_set_channel(struct obs_view *view, size_t channel, obs_source_t *source)
{
	view->channels[channel] = source;
}

void obs_view_clear_channels(struct obs_view *view)
{
	memset(view->channels, 0, sizeof(view->channels));
}

bool obs_view_source_removed(const obs_source_t *source)
{
	return source->removed;
}

struct obs_video_info *obs_view_main_mix_info(void)
{
	if (!obs->data.main_canvas->mix)
		return NULL;
	return &obs->data.main_canvas->mix->ovi;
}

video_t *obs_view_mix_add(struct obs_view *view, struct obs_video_info *ovi)
{
	struct obs_core_video_mix *mix = obs_create_video_mix(ovi);
	if (!mix) {
		return NULL;
	}
	mix->view = view;

	pthread_mutex_lock(&obs->video.mixes_mutex);
	da_push_back(obs->video.mixes, &mix);
	pthread_mutex_unlock(&obs->video.mixes_mutex);

	return mix->video;
}

void obs_view_mixes_lock(void)
{
	pthread_mutex_lock(&obs->video.mixes_mutex);
}

void obs_view_mixes_unlock(void)
{
	pthread_mutex_unlock(&obs->video.mixes_mutex);
}

size_t obs_view_mix_count(void)
{
	return obs->video.mixes.num;
}

struct obs_view *obs_view_mix_view(size_t i)
{
	return obs->video.mixes.array[i]->view;
}

void obs_view_mix_clear_view(size_t i)
{
	obs->video.mixes.array[i]->view = NULL;
}

struct obs_video_info *obs_view_mix_info(size_t i)
{
	return &obs->video.mixes.array[i]->ovi;
}
