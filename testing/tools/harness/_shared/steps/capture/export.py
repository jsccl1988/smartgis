# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# BMP export used by map seed showcases.


def capture_frame(sg, leaf, frame):
    sg.export_bmp(leaf=leaf, frame=frame)


def export_seed_bmp(sg, leaf, frame):
    capture_frame(sg, leaf, frame)
    sg.mark("fit-ok")
    sg.mark("extent-ok")
