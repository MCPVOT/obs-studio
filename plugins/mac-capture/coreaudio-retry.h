/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

/* Retry schedule for the CoreAudio reconnect thread (mac-audio.c.h,
 * obs-rust/obs-studio#1358). Plain C with no Apple headers, so a cmocka
 * test drives it on every host with simulated time. */

#include <stdbool.h>

#include <util/bmem.h>
#include <util/threading.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The longest wait between two reconnect attempts. */
#define COREAUDIO_RETRY_MAX_MS 30000UL

struct coreaudio_retry {
	unsigned long base_ms;
	unsigned long next_ms;
	bool cap_reported;
};

/* Start (or restart, on a device-list or default-device change) the
 * schedule at base_ms. */
static inline void coreaudio_retry_reset(struct coreaudio_retry *r, unsigned long base_ms)
{
	r->base_ms = base_ms;
	r->next_ms = base_ms;
	r->cap_reported = false;
}

/* The wait before the next attempt. *cap_first is set to true exactly once
 * per schedule: the first time the wait reaches COREAUDIO_RETRY_MAX_MS. */
static inline unsigned long coreaudio_retry_next(struct coreaudio_retry *r, bool *cap_first)
{
	unsigned long wait = r->next_ms;

	if (wait >= COREAUDIO_RETRY_MAX_MS) {
		wait = COREAUDIO_RETRY_MAX_MS;
	} else if (r->next_ms > COREAUDIO_RETRY_MAX_MS / 2) {
		r->next_ms = COREAUDIO_RETRY_MAX_MS;
	} else {
		r->next_ms *= 2;
	}

	*cap_first = wait == COREAUDIO_RETRY_MAX_MS && !r->cap_reported;
	if (*cap_first) {
		r->cap_reported = true;
	}
	return wait;
}

/* Wake context shared by the source and the reconnect listeners. The
 * CoreAudio listener callback touches only this object, never the source,
 * and holds its own reference while registered: a callback that still runs
 * after the source let go finds a live context. The last unref destroys the
 * event and frees the context. */
struct coreaudio_wake {
	volatile long refs;
	volatile bool reset;
	os_event_t *event;
};

/* One reference, for the caller (the source). NULL when the event cannot be
 * created. */
static inline struct coreaudio_wake *coreaudio_wake_create(void)
{
	struct coreaudio_wake *w = bzalloc(sizeof(*w));

	if (os_event_init(&w->event, OS_EVENT_TYPE_AUTO) != 0) {
		bfree(w);
		return NULL;
	}
	w->refs = 1;
	return w;
}

static inline void coreaudio_wake_ref(struct coreaudio_wake *w)
{
	os_atomic_inc_long(&w->refs);
}

/* Returns true when this was the last reference and the context is gone. */
static inline bool coreaudio_wake_unref(struct coreaudio_wake *w)
{
	if (os_atomic_dec_long(&w->refs) != 0) {
		return false;
	}
	os_event_destroy(w->event);
	bfree(w);
	return true;
}

/* What the listener callback does: ask for a reset and wake the waiter. */
static inline void coreaudio_wake_signal(struct coreaudio_wake *w)
{
	w->reset = true;
	os_event_signal(w->event);
}

#ifdef __cplusplus
}
#endif
