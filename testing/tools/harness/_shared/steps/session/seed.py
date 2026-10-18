# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# map2d.seed modes. Capture lives in steps.capture.export.


def seed_mode(sg, mode, seed_mark):
    sg.run_processing(id="map2d.seed", args='{"mode":"%s"}' % mode)
    sg.mark(seed_mark)
    sg.mark("map2d-seed-plugin")
