/* The RTMP output's host interface (obs-rust/obs-studio#851): socket
 * errors and ioctls, the root certificate hint, connection logging and the
 * Windows-only send thread ("new socket loop"). Sockets are passed as
 * uintptr_t so the interface needs no socket headers. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct rtmp_stream;

int rtmp_host_last_socket_error(void);
/* FIONREAD: returns the ioctl result, stores the pending byte count. */
int rtmp_host_bytes_available(uintptr_t sock, int *size);
/* FIONBIO: 0, or the socket error. */
int rtmp_host_set_nonblocking(uintptr_t sock);
/* The locale text for a host socket error, or NULL. */
const char *rtmp_host_socket_error_text(int code);
const char *rtmp_host_root_cert_error(void);

void rtmp_host_log_sndbuf_size(struct rtmp_stream *stream);
void rtmp_host_log_interface_type(struct rtmp_stream *stream);

/* The send thread and its settings (new socket loop, low latency mode). */
bool rtmp_host_has_socket_loop(void);
bool rtmp_host_start_socket_loop(struct rtmp_stream *stream);
void *socket_thread_windows(void *data);
