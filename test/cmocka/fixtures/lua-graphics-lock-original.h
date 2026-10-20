/* clang-format off */


/* Frozen characterization baseline from 7a56d867ee7382656804ff26929455073eaf25bf,
 * shared/obs-scripting/obs-scripting-lua-source.c.h:186-199.
 * Kept for #1275: the production regression intentionally changes busy-lock
 * behavior. The existing C characterization assertions remain unchanged. */
static bool lock_in_graphics(pthread_mutex_t *mutex)
{
	if (!gs_get_context()) {
		pthread_mutex_lock(mutex);
		return true;
	}

	while (pthread_mutex_trylock(mutex) != 0) {
		if (os_atomic_load_long(&lua_graphics_waiters) > 0)
			return false;
		os_sleep_ms(1);
	}
	return true;
}
