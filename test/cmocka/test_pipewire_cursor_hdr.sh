#!/bin/sh
# Runs test_pipewire_cursor_hdr against a private PipeWire daemon: its own
# runtime dir and config, no session manager, no devices, and never the
# user's session bus. Exits 77 (skip) when pipewire is not installed or does
# not start.
# usage: test_pipewire_cursor_hdr.sh <test binary>
test_bin=$1

command -v pipewire >/dev/null 2>&1 || {
	echo "pipewire not found: skipping"
	exit 77
}

dir=$(mktemp -d "${TMPDIR:-/tmp}/obs-pw-cursor-test.XXXXXX") || exit 1
export XDG_RUNTIME_DIR="$dir" PIPEWIRE_RUNTIME_DIR="$dir"
export XDG_CONFIG_HOME="$dir/config" XDG_STATE_HOME="$dir/state" HOME="$dir/home"
unset DBUS_SESSION_BUS_ADDRESS PIPEWIRE_REMOTE
mkdir -p "$XDG_CONFIG_HOME" "$XDG_STATE_HOME" "$HOME"

pipewire >"$dir/pipewire.log" 2>&1 &
pw=$!

i=0
until [ -S "$dir/pipewire-0" ]; do
	i=$((i + 1))
	if [ $i -gt 50 ]; then
		echo "private pipewire did not start: skipping"
		kill $pw 2>/dev/null
		rm -rf "$dir"
		exit 77
	fi
	sleep 0.1
done

"$test_bin"
status=$?

kill $pw 2>/dev/null
wait $pw 2>/dev/null
rm -rf "$dir"
exit $status
