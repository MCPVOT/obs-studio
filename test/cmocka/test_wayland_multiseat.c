/* Wayland hotkeys on a compositor with more than one seat (upstream
 * obsproject/obs-studio#12929, reported by chronically-late; first filed as
 * obsproject/obs-studio#12237).
 *
 * The real libobs Wayland hotkey platform (obs-nix-wayland.c, compiled in)
 * is connected over a socketpair to a minimal in-test compositor running in
 * a child process. The compositor advertises two wl_seat globals and treats
 * wl_seat.get_keyboard on a seat that never had the keyboard capability as
 * the protocol error the specification defines (wl_seat.missing_capability),
 * as KWin and other compositors do. On request from the test it also changes
 * a seat's capabilities or removes a seat at runtime. A protocol error is fatal for the whole
 * connection, which OBS shares with Qt, so in OBS it takes the app down.
 *
 * No real display is used: the connection never leaves this test. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include <obs-internal.h>
#include "obs-nix-platform.h"
#include "obs-nix-wayland.h"

struct obs_core *wayland_test_obs;

/* Internal to libobs (obs-hotkey.c); only key names use it. */
const char *obs_get_hotkey_translation(obs_key_t key, const char *def)
{
	(void)key;
	return def;
}

/* ------------------------------------------------------------------------ */
/* Fake compositor (child process)                                          */

#define MAX_SEATS 4

/* Counters the compositor shares with the test process. */
struct compositor_stats {
	int keyboards_requested[MAX_SEATS];
	int live_keyboards[MAX_SEATS];
	int live_seats;
	int missing_capability_errors;
};

static struct compositor_stats *stats;

struct fake_seat {
	int index;
	uint32_t caps;
	const char *name;
	/* Compositor side only. */
	struct wl_global *global;
	struct wl_resource *resource;
};

/* Runtime changes the test asks the compositor for. */
enum control_op {
	CONTROL_SET_CAPS,
	CONTROL_REMOVE_SEAT,
};

struct control {
	enum control_op op;
	int seat;
	uint32_t caps;
};

struct compositor {
	struct wl_display *display;
	struct fake_seat *seats;
	int ack_fd;
};

static bool client_gone;

static void resource_destroy_request(struct wl_client *client, struct wl_resource *resource)
{
	(void)client;
	wl_resource_destroy(resource);
}

static const struct wl_pointer_interface pointer_impl = {
	.set_cursor = NULL,
	.release = resource_destroy_request,
};

static const struct wl_keyboard_interface keyboard_impl = {
	.release = resource_destroy_request,
};

static void keyboard_resource_destroyed(struct wl_resource *resource)
{
	struct fake_seat *seat = wl_resource_get_user_data(resource);
	stats->live_keyboards[seat->index]--;
}

static void seat_get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct fake_seat *seat = wl_resource_get_user_data(resource);
	if (!(seat->caps & WL_SEAT_CAPABILITY_POINTER)) {
		wl_resource_post_error(resource, WL_SEAT_ERROR_MISSING_CAPABILITY, "seat has no pointer");
		return;
	}
	struct wl_resource *pointer =
		wl_resource_create(client, &wl_pointer_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(pointer, &pointer_impl, seat, NULL);
}

static void seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	struct fake_seat *seat = wl_resource_get_user_data(resource);
	stats->keyboards_requested[seat->index]++;
	if (!(seat->caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
		stats->missing_capability_errors++;
		wl_resource_post_error(resource, WL_SEAT_ERROR_MISSING_CAPABILITY,
				       "get_keyboard on seat '%s', which has no keyboard", seat->name);
		return;
	}
	struct wl_resource *keyboard =
		wl_resource_create(client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
	wl_resource_set_implementation(keyboard, &keyboard_impl, seat, keyboard_resource_destroyed);
	stats->live_keyboards[seat->index]++;
}

static void seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
	(void)client;
	(void)id;
	wl_resource_post_error(resource, WL_SEAT_ERROR_MISSING_CAPABILITY, "no touch");
}

static const struct wl_seat_interface seat_impl = {
	.get_pointer = seat_get_pointer,
	.get_keyboard = seat_get_keyboard,
	.get_touch = seat_get_touch,
	.release = resource_destroy_request,
};

static void seat_resource_destroyed(struct wl_resource *resource)
{
	struct fake_seat *seat = wl_resource_get_user_data(resource);
	if (seat->resource == resource)
		seat->resource = NULL;
	stats->live_seats--;
}

static void bind_seat(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct fake_seat *seat = data;
	struct wl_resource *resource = wl_resource_create(client, &wl_seat_interface, (int)version, id);
	wl_resource_set_implementation(resource, &seat_impl, seat, seat_resource_destroyed);
	seat->resource = resource;
	stats->live_seats++;
	wl_seat_send_capabilities(resource, seat->caps);
	if (version >= WL_SEAT_NAME_SINCE_VERSION)
		wl_seat_send_name(resource, seat->name);
}

static void client_destroyed(struct wl_listener *listener, void *data)
{
	(void)listener;
	(void)data;
	client_gone = true;
}

static int handle_control(int fd, uint32_t mask, void *data)
{
	(void)mask;
	struct compositor *c = data;
	struct control ctl;
	if (read(fd, &ctl, sizeof(ctl)) != sizeof(ctl)) {
		client_gone = true;
		return 0;
	}

	struct fake_seat *seat = &c->seats[ctl.seat];
	if (ctl.op == CONTROL_SET_CAPS) {
		seat->caps = ctl.caps;
		if (seat->resource)
			wl_seat_send_capabilities(seat->resource, seat->caps);
	} else {
		/* Clients still holding the seat may use it until they release
		 * it; the global itself goes away with the display. */
		wl_global_remove(seat->global);
	}
	wl_display_flush_clients(c->display);
	char ack = 1;
	if (write(c->ack_fd, &ack, 1) != 1)
		client_gone = true;
	return 0;
}

static void run_compositor(int fd, int control_fd, int ack_fd, struct fake_seat *seats, size_t num_seats)
{
	struct wl_display *display = wl_display_create();
	for (size_t i = 0; i < num_seats; i++)
		seats[i].global = wl_global_create(display, &wl_seat_interface, 7, &seats[i], bind_seat);

	struct wl_client *client = wl_client_create(display, fd);
	struct wl_listener destroy_listener = {.notify = client_destroyed};
	wl_client_add_destroy_listener(client, &destroy_listener);

	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	struct compositor c = {.display = display, .seats = seats, .ack_fd = ack_fd};
	wl_event_loop_add_fd(loop, control_fd, WL_EVENT_READABLE, handle_control, &c);
	while (!client_gone) {
		wl_display_flush_clients(display);
		if (wl_event_loop_dispatch(loop, 5000) < 0)
			break;
	}
	wl_display_destroy(display);
	_exit(0);
}

/* ------------------------------------------------------------------------ */
/* Test process                                                             */

struct session {
	pid_t compositor;
	int control_fd;
	int ack_fd;
	struct wl_display *display;
	struct obs_core_hotkeys hotkeys;
	const struct obs_nix_hotkeys_vtable *vtable;
};

static void session_start(struct session *s, struct fake_seat *seats, size_t num_seats)
{
	memset(stats, 0, sizeof(*stats));
	memset(s, 0, sizeof(*s));

	int fds[2], control[2], ack[2];
	assert_int_equal(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);
	assert_int_equal(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, control), 0);
	assert_int_equal(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ack), 0);
	s->compositor = fork();
	assert_true(s->compositor >= 0);
	if (s->compositor == 0) {
		close(fds[0]);
		close(control[1]);
		close(ack[0]);
		run_compositor(fds[1], control[0], ack[1], seats, num_seats);
	}
	close(fds[1]);
	close(control[0]);
	close(ack[1]);
	s->control_fd = control[1];
	s->ack_fd = ack[0];

	s->display = wl_display_connect_to_fd(fds[0]);
	assert_non_null(s->display);
	obs_set_nix_platform(OBS_NIX_PLATFORM_WAYLAND);
	obs_set_nix_platform_display(s->display);

	s->vtable = obs_nix_wayland_get_hotkeys_vtable();
	assert_true(s->vtable->init(&s->hotkeys));
	/* OBS shares the connection with Qt, whose event loop dispatches the
	 * seat events that the registry roundtrip in init() asked for. */
	wl_display_roundtrip(s->display);
	wl_display_roundtrip(s->display);
}

static void assert_connection_alive(struct session *s)
{
	int err = wl_display_get_error(s->display);
	if (err != 0) {
		const struct wl_interface *iface = NULL;
		uint32_t id = 0;
		uint32_t code = wl_display_get_protocol_error(s->display, &iface, &id);
		fprintf(stderr,
			"Wayland connection died (errno %d): protocol error %u on %s@%u; "
			"get_keyboard per seat: [%d, %d], missing_capability errors: %d\n",
			err, code, iface ? iface->name : "?", id, stats->keyboards_requested[0],
			stats->keyboards_requested[1], stats->missing_capability_errors);
		fail();
	}
}

/* Apply a runtime change in the compositor, then dispatch the events it
 * sent, as Qt's event loop would. */
static void session_control(struct session *s, enum control_op op, int seat, uint32_t caps)
{
	struct control ctl = {.op = op, .seat = seat, .caps = caps};
	assert_int_equal(write(s->control_fd, &ctl, sizeof(ctl)), sizeof(ctl));
	char ack;
	assert_int_equal(read(s->ack_fd, &ack, 1), 1);
	wl_display_roundtrip(s->display);
	wl_display_roundtrip(s->display);
}

static void session_stop(struct session *s)
{
	wl_display_disconnect(s->display);
	close(s->control_fd);
	close(s->ack_fd);
	int status = 0;
	waitpid(s->compositor, &status, 0);
	obs_set_nix_platform_display(NULL);
}

/* Free the platform, then let the compositor see the requests. */
static void session_free_platform(struct session *s)
{
	s->vtable->free(&s->hotkeys);
	wl_display_roundtrip(s->display);
}

/* A keyboard seat followed by a seat without one, e.g. a second seat that
 * only has a pointer or no devices yet. The keyboard must be requested from
 * the seat that announced it, not from whichever seat was bound last. */
static void keyboard_seat_then_pointer_seat(void **state)
{
	(void)state;
	struct fake_seat seats[] = {
		{0, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER, "seat0"},
		{1, WL_SEAT_CAPABILITY_POINTER, "seat1"},
	};
	struct session s;
	session_start(&s, seats, 2);

	assert_connection_alive(&s);
	if (stats->missing_capability_errors != 0 || stats->live_keyboards[0] != 1) {
		fprintf(stderr, "expected one keyboard on seat0, got seat0=%d seat1=%d (requests %d/%d)\n",
			stats->live_keyboards[0], stats->live_keyboards[1], stats->keyboards_requested[0],
			stats->keyboards_requested[1]);
		fail();
	}

	session_free_platform(&s);
	assert_connection_alive(&s);
	session_stop(&s);
}

/* A later seat with no keyboard must not take away the keyboard that came
 * from another seat. */
static void empty_seat_does_not_release_keyboard(void **state)
{
	(void)state;
	struct fake_seat seats[] = {
		{0, WL_SEAT_CAPABILITY_POINTER, "seat-pointer"},
		{1, WL_SEAT_CAPABILITY_KEYBOARD, "seat-keyboard"},
		{2, 0, "seat-empty"},
	};
	struct session s;
	session_start(&s, seats, 3);

	assert_connection_alive(&s);
	if (stats->live_keyboards[1] != 1) {
		fprintf(stderr, "keyboard from seat-keyboard is gone: live=%d requested=%d\n", stats->live_keyboards[1],
			stats->keyboards_requested[1]);
		fail();
	}

	session_free_platform(&s);
	assert_connection_alive(&s);
	session_stop(&s);
}

/* Every seat the platform bound is released when hotkeys shut down. */
static void free_releases_every_seat(void **state)
{
	(void)state;
	struct fake_seat seats[] = {
		{0, WL_SEAT_CAPABILITY_POINTER, "seat0"},
		{1, WL_SEAT_CAPABILITY_KEYBOARD, "seat1"},
	};
	struct session s;
	session_start(&s, seats, 2);
	assert_connection_alive(&s);
	assert_int_equal(stats->live_seats, 2);

	session_free_platform(&s);
	assert_connection_alive(&s);
	if (stats->live_seats != 0 || stats->live_keyboards[1] != 0) {
		fprintf(stderr, "after free: %d seat(s) and %d keyboard(s) still bound\n", stats->live_seats,
			stats->live_keyboards[1]);
		fail();
	}
	session_stop(&s);
}

/* A seat removed at runtime (the compositor's global_remove) is released,
 * and when it is the keyboard's seat another seat's keyboard takes over. */
static void removed_keyboard_seat_hands_over(void **state)
{
	(void)state;
	struct fake_seat seats[] = {
		{0, WL_SEAT_CAPABILITY_KEYBOARD, "seat0"},
		{1, WL_SEAT_CAPABILITY_KEYBOARD, "seat1"},
	};
	struct session s;
	session_start(&s, seats, 2);
	assert_connection_alive(&s);

	session_control(&s, CONTROL_REMOVE_SEAT, 0, 0);
	assert_connection_alive(&s);
	if (stats->live_seats != 1 || stats->live_keyboards[0] != 0 || stats->live_keyboards[1] != 1) {
		fprintf(stderr,
			"after removing seat0: %d seat(s) bound, keyboards seat0=%d seat1=%d "
			"(expected 1 seat, keyboard on seat1 only)\n",
			stats->live_seats, stats->live_keyboards[0], stats->live_keyboards[1]);
		fail();
	}

	session_free_platform(&s);
	assert_connection_alive(&s);
	assert_int_equal(stats->live_seats, 0);
	session_stop(&s);
}

/* The keyboard's seat losing its keyboard releases it, and a seat that
 * gains one later provides it again. */
static void keyboard_moves_between_seats(void **state)
{
	(void)state;
	struct fake_seat seats[] = {
		{0, WL_SEAT_CAPABILITY_POINTER, "seat0"},
		{1, WL_SEAT_CAPABILITY_KEYBOARD, "seat1"},
	};
	struct session s;
	session_start(&s, seats, 2);
	assert_connection_alive(&s);

	session_control(&s, CONTROL_SET_CAPS, 1, WL_SEAT_CAPABILITY_POINTER);
	assert_connection_alive(&s);
	if (stats->keyboards_requested[1] != 1 || stats->live_keyboards[1] != 0) {
		fprintf(stderr, "seat1 lost its keyboard: requested %d, still bound %d (expected 1 and 0)\n",
			stats->keyboards_requested[1], stats->live_keyboards[1]);
		fail();
	}
	session_control(&s, CONTROL_SET_CAPS, 0, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
	assert_connection_alive(&s);
	if (stats->live_keyboards[0] != 1 || stats->live_keyboards[1] != 0) {
		fprintf(stderr, "keyboards seat0=%d seat1=%d, expected the keyboard on seat0 only\n",
			stats->live_keyboards[0], stats->live_keyboards[1]);
		fail();
	}

	session_free_platform(&s);
	assert_connection_alive(&s);
	session_stop(&s);
}

/* The single-seat case keeps working. */
static void single_seat(void **state)
{
	(void)state;
	struct fake_seat seats[] = {
		{0, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER, "seat0"},
	};
	struct session s;
	session_start(&s, seats, 1);
	assert_connection_alive(&s);
	assert_int_equal(stats->live_keyboards[0], 1);
	session_free_platform(&s);
	assert_connection_alive(&s);
	assert_int_equal(stats->live_seats, 0);
	session_stop(&s);
}

int main(void)
{
	signal(SIGPIPE, SIG_IGN);
	stats = mmap(NULL, sizeof(*stats), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (stats == MAP_FAILED)
		return 1;
	wayland_test_obs = bzalloc(sizeof(*wayland_test_obs));

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(single_seat),
		cmocka_unit_test(keyboard_seat_then_pointer_seat),
		cmocka_unit_test(empty_seat_does_not_release_keyboard),
		cmocka_unit_test(free_releases_every_seat),
		cmocka_unit_test(removed_keyboard_seat_hands_over),
		cmocka_unit_test(keyboard_moves_between_seats),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
