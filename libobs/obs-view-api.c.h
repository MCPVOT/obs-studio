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

#include "libobs/build/libobs-ported-view.pch.h"

/* Views, moved from obs-view.c (#1933): the channel table of a view, its
 * activation of the sources it holds, and its video mixes. struct obs_view
 * stays in obs-view.c, embedded in struct obs_canvas, so these reach it only
 * through the hidden accessors there (declared in obs-internal.h), and the
 * mixes list through the hidden obs_view_mix*() helpers beside them. The
 * Rust port (obs-core's view module) replaces this file. test_view pins what
 * they do, including that obs_view_render() drops a removed source without
 * deactivating it. */

bool obs_view_init(struct obs_view *view, enum view_type type)
{
	if (!view)
		return false;

	if (!obs_view_mutex_init(view)) {
		blog(LOG_ERROR, "obs_view_init: Failed to create mutex");
		return false;
	}

	obs_view_set_type(view, type);
	return true;
}

obs_view_t *obs_view_create(void)
{
	struct obs_view *view = bzalloc(obs_view_struct_size());

	if (!obs_view_init(view, AUX_VIEW)) {
		bfree(view);
		view = NULL;
	}

	return view;
}

void obs_view_free(struct obs_view *view)
{
	if (!view)
		return;

	for (size_t i = 0; i < MAX_CHANNELS; i++) {
		struct obs_source *source = obs_view_channel(view, i);
		if (source) {
			obs_source_deactivate(source, obs_view_get_type(view));
			obs_source_release(source);
		}
	}

	obs_view_clear_channels(view);
	obs_view_mutex_destroy(view);
}

void obs_view_destroy(obs_view_t *view)
{
	if (view) {
		obs_view_free(view);
		bfree(view);
	}
}

obs_source_t *obs_view_get_source(obs_view_t *view, uint32_t channel)
{
	obs_source_t *source;
	assert(channel < MAX_CHANNELS);

	if (!view)
		return NULL;
	if (channel >= MAX_CHANNELS)
		return NULL;

	obs_view_lock(view);
	source = obs_source_get_ref(obs_view_channel(view, channel));
	obs_view_unlock(view);

	return source;
}

void obs_view_set_source(obs_view_t *view, uint32_t channel, obs_source_t *source)
{
	struct obs_source *prev_source;

	assert(channel < MAX_CHANNELS);

	if (!view)
		return;
	if (channel >= MAX_CHANNELS)
		return;

	obs_view_lock(view);
	source = obs_source_get_ref(source);
	prev_source = obs_view_channel(view, channel);
	obs_view_set_channel(view, channel, source);

	obs_view_unlock(view);

	if (source)
		obs_source_activate(source, obs_view_get_type(view));

	if (prev_source) {
		obs_source_deactivate(prev_source, obs_view_get_type(view));
		obs_source_release(prev_source);
	}
}

void obs_view_render(obs_view_t *view)
{
	if (!view)
		return;

	obs_view_lock(view);

	for (size_t i = 0; i < MAX_CHANNELS; i++) {
		struct obs_source *source;

		source = obs_view_channel(view, i);

		if (source) {
			if (obs_view_source_removed(source)) {
				obs_source_release(source);
				obs_view_set_channel(view, i, NULL);
			} else {
				obs_source_video_render(source);
			}
		}
	}

	obs_view_unlock(view);
}

video_t *obs_view_add(obs_view_t *view)
{
	struct obs_video_info *ovi = obs_view_main_mix_info();
	if (!ovi)
		return NULL;
	return obs_view_add2(view, ovi);
}

video_t *obs_view_add2(obs_view_t *view, struct obs_video_info *ovi)
{
	if (!view || !ovi)
		return NULL;

	return obs_view_mix_add(view, ovi);
}

void obs_view_remove(obs_view_t *view)
{
	if (!view)
		return;

	obs_view_mixes_lock();
	for (size_t i = 0, num = obs_view_mix_count(); i < num; i++) {
		if (obs_view_mix_view(i) == view)
			obs_view_mix_clear_view(i);
	}
	obs_view_mixes_unlock();
}

void obs_view_enum_video_info(obs_view_t *view, bool (*enum_proc)(void *, struct obs_video_info *), void *param)
{
	obs_view_mixes_lock();

	for (size_t i = 0, num = obs_view_mix_count(); i < num; i++) {
		if (obs_view_mix_view(i) != view)
			continue;
		if (!enum_proc(param, obs_view_mix_info(i)))
			break;
	}

	obs_view_mixes_unlock();
}
