# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Regenerates testing/data/plugin/orthogrid_sample.gridbnd — complex coastal bay.

from __future__ import annotations

import math
from pathlib import Path

N = 33
SW, SE, NE, NW = (0.00, 0.00), (1.00, 0.00), (1.00, 1.00), (0.00, 1.00)
COUNTS = {0: 17, 1: 15, 2: 17, 3: 15}


def sample_edge(flag: int, count: int) -> list[tuple[float, float]]:
    pts: list[tuple[float, float]] = []
    for k in range(count):
        t = k / (count - 1)
        if flag == 0:  # south: open sea, embayments inland (+y)
            x = t
            y = (
                0.055 * math.sin(t * math.pi * 3.0)
                + 0.028 * math.sin(t * math.pi * 7.0 + 0.4)
                + 0.018 * abs(math.sin(t * math.pi * 2.0))
                + 0.07 * math.exp(-((t - 0.28) ** 2) / 0.012)
                + 0.045 * math.exp(-((t - 0.72) ** 2) / 0.010)
            )
            pts.append((x, max(0.0, y)))
        elif flag == 1:  # east: rocky headland / cape
            y = t
            x = (
                1.0
                - 0.06 * math.sin(t * math.pi)
                - 0.04 * math.sin(t * math.pi * 4.0)
                - 0.09 * math.exp(-((t - 0.55) ** 2) / 0.008)
                - 0.035 * math.exp(-((t - 0.22) ** 2) / 0.006)
            )
            pts.append((min(1.0, x), y))
        elif flag == 2:  # north: mainland + estuary (east→west)
            x = 1.0 - t
            y = (
                1.0
                - 0.05 * math.sin(t * math.pi * 2.5)
                - 0.11 * math.exp(-((t - 0.42) ** 2) / 0.015)
                - 0.04 * math.sin(t * math.pi * 6.0 + 1.2)
            )
            pts.append((x, min(1.0, y)))
        else:  # west: lagoon pocket (north→south)
            y = 1.0 - t
            x = (
                0.08 * math.sin(t * math.pi)
                + 0.05 * math.sin(t * math.pi * 3.0 + 0.6)
                + 0.10 * math.exp(-((t - 0.35) ** 2) / 0.014)
            )
            pts.append((max(0.0, x), y))
    if flag == 0:
        pts[0], pts[-1] = SW, SE
    elif flag == 1:
        pts[0], pts[-1] = SE, NE
    elif flag == 2:
        pts[0], pts[-1] = NE, NW
    else:
        pts[0], pts[-1] = NW, SW
    return pts


def main() -> None:
    last = N - 1
    meta = [
        (0, 0, last, 0),
        (1, 0, last, last),
        (2, last, 0, last),
        (3, last, 0, 0),
    ]
    lines = ["gridbnd:", f"{N} {N}", "begin", "main_begin"]
    for flag, start, end, index in meta:
        pts = sample_edge(flag, COUNTS[flag])
        lines.append(str(len(pts)))
        lines.append(f"{start} {end} {index} {flag} 1 1")
        for x, y in pts:
            lines.append(f"{x:.6f},{y:.6f}")
    lines += ["main_end", "end"]
    path = Path(__file__).resolve().parent / "plugin" / "orthogrid_sample.gridbnd"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path} grid={N}x{N}")


if __name__ == "__main__":
    main()
