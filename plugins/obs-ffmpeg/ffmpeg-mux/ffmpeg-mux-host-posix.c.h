#include "plugins/obs-ffmpeg/ffmpeg-mux/build/ffmpeg-mux-posix.pch.h"

/* The ffmpeg-mux helper's macOS and Linux entry point (#853): arguments are
 * already UTF-8 and stdin is binary, so main hands them straight over. */

int main(int argc, char *argv[])
{
	return ffmpeg_mux_main(argc, argv);
}
