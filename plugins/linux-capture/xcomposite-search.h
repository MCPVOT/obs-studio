/* When the XComposite window capture searches for its window again, and
 * whether a search result needs a new pixmap and watcher (#1357).
 *
 * Pure: time and window state come in as arguments, so the decision is
 * tested without an X server (test/cmocka/test_xcomposite_search_backoff.c). */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define XCOMP_SEARCH_INTERVAL_MIN 2.0
#define XCOMP_SEARCH_INTERVAL_MAX 30.0

struct xcomp_search {
	double since_search;
	double interval;
};

/* Forget the backoff: the next search waits the shortest interval. */
static inline void xcomp_search_reset(struct xcomp_search *st)
{
	st->since_search = 0.0;
	st->interval = XCOMP_SEARCH_INTERVAL_MIN;
}

/* Called every tick. True when the window search runs now: at once when the
 * window changed (an X event on it or new settings), else when the window is
 * lost and the current interval has passed. */
static inline bool xcomp_search_due(struct xcomp_search *st, double seconds, bool window_lost, bool forced)
{
	if (st->interval < XCOMP_SEARCH_INTERVAL_MIN)
		st->interval = XCOMP_SEARCH_INTERVAL_MIN;
	st->since_search += seconds;
	if (forced)
		return true;
	return window_lost && st->since_search > st->interval;
}

/* After a search: true when the pixmap and the watcher must be rebuilt. A
 * search that found the same window, still not viewable, changes nothing:
 * the watcher already listens on it and a pixmap cannot be made. */
static inline bool xcomp_search_needs_rebuild(uint32_t old_win, uint32_t found_win, bool found_viewable, bool forced)
{
	return forced || found_win != old_win || found_viewable;
}

/* After a search: captured is whether a texture exists now. A capture or a
 * forced search starts the interval over; another miss doubles it, up to
 * the cap. */
static inline void xcomp_search_done(struct xcomp_search *st, bool captured, bool forced)
{
	st->since_search = 0.0;
	if (captured || forced) {
		st->interval = XCOMP_SEARCH_INTERVAL_MIN;
		return;
	}
	st->interval *= 2.0;
	if (st->interval > XCOMP_SEARCH_INTERVAL_MAX)
		st->interval = XCOMP_SEARCH_INTERVAL_MAX;
}
