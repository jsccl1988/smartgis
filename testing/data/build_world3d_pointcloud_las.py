#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Write a tiny LAS 1.2 (format 0) fixture for world3d pointcloud tests.

Output: testing/data/plugin/world3d_pointcloud_sample.las (3 points).
"""

from __future__ import annotations

import struct
from pathlib import Path

OUT = Path(__file__).resolve().parent / "plugin" / "world3d_pointcloud_sample.las"


def main() -> int:
    buf = bytearray()
    buf += b"LASF"
    buf += bytes(20)  # 4..23
    buf += bytes([1, 2])  # version
    buf += bytes(68)  # through 93
    buf += struct.pack("<H", 227)  # header size
    buf += struct.pack("<I", 227)  # offset to points
    buf += struct.pack("<I", 0)  # VLRs
    buf += bytes([0])  # format 0
    buf += struct.pack("<H", 20)  # point length
    buf += struct.pack("<I", 3)  # count
    while len(buf) < 131:
        buf.append(0)
    buf += struct.pack("<ddd", 0.01, 0.01, 0.01)  # scale
    buf += struct.pack("<ddd", 0.0, 0.0, 0.0)  # offset
    while len(buf) < 227:
        buf.append(0)
    # Geographic-ish sample around China box (lon, lat, elev m → scaled ints).
    # x=105.00, y=35.00, z=100 → ints 10500, 3500, 10000 with scale 0.01
    samples = [
        (10500, 3500, 10000),
        (10510, 3510, 10100),
        (10520, 3520, 10200),
    ]
    for x, y, z in samples:
        buf += struct.pack("<iii", x, y, z)
        buf += struct.pack("<H", 0)  # intensity
        buf += bytes([0, 1, 0, 0])  # return/class/scan/user
        buf += struct.pack("<H", 0)  # source

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(bytes(buf))
    print(f"wrote {OUT} ({len(buf)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
