#include "plugins/obs-outputs/build/obs-outputs-host-windows.pch.h"

/* obs-outputs' Windows module host (#851): Winsock for the module's
 * lifetime, and Win32 critical sections for mbedtls when it is built with
 * MBEDTLS_THREADING_ALT. */

#ifdef MBEDTLS_THREADING_ALT
void mbed_mutex_init(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = bzalloc(sizeof(CRITICAL_SECTION));
	*m = c;
	InitializeCriticalSection(c);
}

void mbed_mutex_free(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = *m;
	DeleteCriticalSection(c);
	bfree(*m);
	*m = NULL;
}

int mbed_mutex_lock(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = *m;
	EnterCriticalSection(c);
	return 0;
}

int mbed_mutex_unlock(mbedtls_threading_mutex_t *m)
{
	CRITICAL_SECTION *c = *m;
	LeaveCriticalSection(c);
	return 0;
}
#endif

void obs_outputs_host_load(void)
{
	WSADATA wsad;
	WSAStartup(MAKEWORD(2, 2), &wsad);

#ifdef MBEDTLS_THREADING_ALT
	mbedtls_threading_set_alt(mbed_mutex_init, mbed_mutex_free, mbed_mutex_lock, mbed_mutex_unlock);
#endif
}

void obs_outputs_host_unload(void)
{
#ifdef MBEDTLS_THREADING_ALT
	mbedtls_threading_free_alt();
#endif
	WSACleanup();
}
