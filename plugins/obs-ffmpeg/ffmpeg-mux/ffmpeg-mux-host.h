/* The ffmpeg-mux helper's host interface (obs-rust/obs-studio#853). Each
 * host file supplies the process entry point (main, or wmain on Windows),
 * prepares the process and hands UTF-8 arguments to ffmpeg_mux_main. */

#pragma once

int ffmpeg_mux_main(int argc, char *argv[]);
