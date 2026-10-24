#include "plugins/obs-outputs/build/obs-outputs-rtmp-host-posix.pch.h"

/* The RTMP output's macOS and Linux host (#851): errno and ioctl, no send
 * thread. */

int rtmp_host_last_socket_error(void)
{
	return errno;
}

int rtmp_host_bytes_available(uintptr_t sock, int *size)
{
	return ioctl((int)sock, FIONREAD, size);
}

int rtmp_host_set_nonblocking(uintptr_t sock)
{
	int one = 1;
	if (ioctl((int)sock, FIONBIO, &one))
		return errno;
	return 0;
}

const char *rtmp_host_socket_error_text(int code)
{
	const char *msg = NULL;
	switch (code) {
	case ETIMEDOUT:
		msg = obs_module_text("ConnectionTimedOut");
		break;
	case EACCES:
		msg = obs_module_text("PermissionDenied");
		break;
	case ECONNABORTED:
		msg = obs_module_text("ConnectionAborted");
		break;
	case ECONNRESET:
		msg = obs_module_text("ConnectionReset");
		break;
	case HOST_NOT_FOUND:
		msg = obs_module_text("HostNotFound");
		break;
	case NO_DATA:
		msg = obs_module_text("NoData");
		break;
	case EADDRNOTAVAIL:
		msg = obs_module_text("AddressNotAvailable");
		break;
	case EINVAL:
		msg = obs_module_text("InvalidParameter");
		break;
	case EHOSTUNREACH:
		msg = obs_module_text("NoRoute");
		break;
	}
	return msg;
}

const char *rtmp_host_root_cert_error(void)
{
	return "Failed to load root certificates for a secure TLS connection."
#ifdef __linux__
	       " Check you have an up to date root certificate bundle in /etc/ssl/certs."
#endif
		;
}

void rtmp_host_log_sndbuf_size(struct rtmp_stream *stream)
{
	(void)stream;
}

void rtmp_host_log_interface_type(struct rtmp_stream *stream)
{
	(void)stream;
}

bool rtmp_host_has_socket_loop(void)
{
	return false;
}

bool rtmp_host_start_socket_loop(struct rtmp_stream *stream)
{
	(void)stream;
	blog(LOG_WARNING, "[rtmp stream] New socket loop not supported on this platform");
	return false;
}
