#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow==12.3.0"]
# ///
"""Regenerate the image-file-* fixtures of test_image_file with Pillow.

The pictures are a few synthetic pixels written here, so they carry no
third-party data. Pillow is needed only to regenerate them, not to build or
run the tests: `uv run test/cmocka/data/image-file.py` rewrites them byte for
byte. Rust tests in rust/obs-graphics read the same files.

- image-file-anim.gif: 4x2, three frames, 10 cs / 0 cs / 5 cs, loops forever.
  Pixel (0, 0) of the first frame is transparent.
- image-file-loop2.gif: the same frames, NETSCAPE loop count 2.
- image-file-noloop.gif: the same frames, no NETSCAPE block (plays once).
- image-file-still.gif: one 2x2 frame.
- image-file-wide.gif: 4097x1, two frames (wider than the 4096 limit).
- image-file.png: 3x2 RGBA, one half-transparent pixel.
"""

from pathlib import Path

from PIL import Image

here = Path(__file__).resolve().parent

PALETTE = [
    0, 0, 0,  # 0 transparent
    255, 0, 0,  # 1
    0, 255, 0,  # 2
    0, 0, 255,  # 3
    200, 100, 50,  # 4
]
PALETTE += [0] * (768 - len(PALETTE))


def frame(width, height, pixels):
    img = Image.new("P", (width, height))
    img.putpalette(PALETTE)
    img.putdata(pixels)
    return img


def anim_frames():
    return [
        frame(4, 2, [0, 1, 1, 1, 1, 1, 1, 4]),
        frame(4, 2, [2, 2, 2, 2, 2, 2, 2, 4]),
        frame(4, 2, [3, 3, 3, 3, 3, 3, 3, 4]),
    ]


def save_gif(name, frames, durations, **extra):
    first, *rest = frames
    first.save(
        here / name,
        save_all=True,
        append_images=rest,
        duration=durations,
        disposal=1,
        transparency=0,
        optimize=False,
        **extra,
    )


save_gif("image-file-anim.gif", anim_frames(), [100, 0, 50], loop=0)
save_gif("image-file-loop2.gif", anim_frames(), [100, 0, 50], loop=2)
save_gif("image-file-noloop.gif", anim_frames(), [100, 0, 50])
frame(2, 2, [1, 2, 3, 4]).save(here / "image-file-still.gif", optimize=False)
save_gif(
    "image-file-wide.gif",
    [frame(4097, 1, [1] * 4097), frame(4097, 1, [2] * 4097)],
    [100, 100],
    loop=0,
)

png = Image.new("RGBA", (3, 2))
png.putdata(
    [
        (255, 0, 0, 255),
        (0, 255, 0, 128),
        (0, 0, 255, 255),
        (10, 20, 30, 255),
        (40, 50, 60, 0),
        (70, 80, 90, 255),
    ]
)
png.save(here / "image-file.png", optimize=False)
