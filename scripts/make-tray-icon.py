"""Wraps a PNG into a single-image .ico (PNG-compressed ICO entries are supported since Windows Vista).

Usage: python scripts/make-tray-icon.py <input.png> <output.ico>
"""
import pathlib
import struct
import sys

src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
png = src.read_bytes()
if png[:8] != b"\x89PNG\r\n\x1a\n":
    sys.exit(f"{src} is not a PNG")
width, height = struct.unpack(">II", png[16:24])
if width > 256 or height > 256:
    sys.exit("icon images must be at most 256x256")
header = struct.pack("<HHH", 0, 1, 1)  # reserved, type 1 = icon, 1 image
entry = struct.pack("<BBBBHHII", width % 256, height % 256, 0, 0, 1, 32, len(png), 6 + 16)
dst.write_bytes(header + entry + png)
print(f"{dst}: {width}x{height}, {len(png)} bytes of PNG")
