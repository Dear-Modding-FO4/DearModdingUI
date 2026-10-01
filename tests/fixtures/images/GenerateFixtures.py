"""Generate tiny, lossless PNG/BC1 comparison art and decoder failure fixtures."""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).parent


def png(name, width, height, pixels):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    ROOT.joinpath(name).write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


colors = [(132, 0, 0, 255), (0, 130, 0, 255), (0, 0, 132, 255), (132, 130, 0, 255)]
png("Tiles.png", 8, 8, bytes(v for y in range(8) for x in range(8)
                           for v in colors[(y // 4) * 2 + x // 4]))
png("Alpha.png", 2, 2, bytes([180, 90, 30, 128, 40, 120, 200, 64,
                            70, 80, 90, 0, 15, 25, 35, 255]))
header = [124, 0xA1007, 8, 8, 32, 0, 4] + [0] * 11
header += [32, 4, int.from_bytes(b"DX10", "little"), 0, 0, 0, 0, 0]
header += [0x401008, 0, 0, 0, 0]
blocks = b"".join(struct.pack("<HHI", value, 0, 0)
                  for value in [0x8000, 0x0400, 0x0010, 0x8400, 0x8000, 0x8000, 0x8000])
for name, cube in [("Tiles.dds", 0), ("Cube.dds", 4)]:
    ROOT.joinpath(name).write_bytes(b"DDS " + struct.pack("<31I", *header)
                                   + struct.pack("<5I", 72, 3, cube, 1, 1) + blocks)
ROOT.joinpath("Unsupported.bin").write_bytes(b"Not an image format.\n")
