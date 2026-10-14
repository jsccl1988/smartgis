# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Unit tests for offline carto_prep helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

# Allow `py -3 testing/data/carto_prep/test_carto_prep.py` and unittest discover.
_DATA = Path(__file__).resolve().parents[1]
if str(_DATA) not in sys.path:
    sys.path.insert(0, str(_DATA))

from carto_prep.bbox import clip_segment_to_bbox, pt_in_bbox
from carto_prep.land_mask import clip_line_to_land_rings, point_in_rings
from carto_prep.normalize import normalize_hydro_props, normalize_road_props


class BboxTests(unittest.TestCase):
    def test_pt_in_bbox(self) -> None:
        box = (73.0, 18.0, 135.0, 54.0)
        self.assertTrue(pt_in_bbox(100.0, 30.0, box))
        self.assertFalse(pt_in_bbox(10.0, 30.0, box))

    def test_clip_keeps_inside_run(self) -> None:
        box = (0.0, 0.0, 10.0, 10.0)
        coords = [[-1, 5], [1, 5], [2, 5], [11, 5]]
        runs = clip_segment_to_bbox(coords, box)
        self.assertEqual(len(runs), 1)
        self.assertEqual(runs[0], [[1.0, 5.0], [2.0, 5.0]])


class NormalizeTests(unittest.TestCase):
    def test_road_omt_classes(self) -> None:
        self.assertEqual(
            normalize_road_props({"kind": "road", "class": "motorway"})["class"],
            "motorway",
        )
        self.assertEqual(
            normalize_road_props({"kind": "road", "class": "trunk"})["class"],
            "trunk",
        )
        self.assertEqual(
            normalize_road_props({"kind": "road", "class": "primary"})["class"],
            "primary",
        )
        # Unknown / empty → primary floor.
        self.assertEqual(
            normalize_road_props({"kind": "road", "class": "residential"})["class"],
            "primary",
        )

    def test_hydro_major_and_stream(self) -> None:
        major = normalize_hydro_props({"kind": "river", "name": "长江", "class": ""})
        self.assertEqual(major["class"], "river")
        other = normalize_hydro_props({"kind": "river", "name": "某支流", "class": ""})
        self.assertEqual(other["class"], "stream")
        lake = normalize_hydro_props({"kind": "lake", "name": "鄱阳湖", "class": ""})
        self.assertEqual(lake["kind"], "lake")
        self.assertEqual(lake["class"], "lake")


class LandMaskTests(unittest.TestCase):
    def test_point_in_square(self) -> None:
        ring = [[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0], [0.0, 0.0]]
        self.assertTrue(point_in_rings(5.0, 5.0, [ring]))
        self.assertFalse(point_in_rings(20.0, 5.0, [ring]))

    def test_clip_drops_ocean_stub(self) -> None:
        ring = [[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0], [0.0, 0.0]]
        # Mostly outside; only a short graze inside → drop.
        coords = [[20.0, 5.0], [21.0, 5.0], [9.0, 5.0], [22.0, 5.0]]
        out = clip_line_to_land_rings(coords, [ring], min_on_land_frac=0.25)
        self.assertEqual(out, [])

    def test_clip_keeps_on_land_run(self) -> None:
        ring = [[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0], [0.0, 0.0]]
        coords = [[1.0, 5.0], [2.0, 5.0], [3.0, 5.0], [4.0, 5.0]]
        out = clip_line_to_land_rings(coords, [ring], min_on_land_frac=0.25)
        self.assertEqual(len(out), 1)
        self.assertGreaterEqual(len(out[0]), 2)


if __name__ == "__main__":
    unittest.main()
