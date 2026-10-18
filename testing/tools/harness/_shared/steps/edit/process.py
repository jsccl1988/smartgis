# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Buffer/clip through run_processing on a tiny fixture.
# Atmosphere hooks live in steps.scene.atmosphere.
# Optimistic conflict lives in edit_conflict_test.

import json
import os
import tempfile

from steps.probe import require_count
from steps.session.document import open_path
from steps.support import slash, write_text

_SQUARE = (
    '{"type":"FeatureCollection","features":[{"type":"Feature",'
    '"properties":{},"geometry":{"type":"Polygon","coordinates":'
    "[[[0,0],[2,0],[2,2],[0,2],[0,0]]]}}]}"
)
_CLIP = (
    '{"type":"FeatureCollection","features":[{"type":"Feature",'
    '"properties":{},"geometry":{"type":"Polygon","coordinates":'
    "[[[1,1],[3,1],[3,3],[1,3],[1,1]]]}}]}"
)


def buffer_and_clip(
    sg, buffer_mark, clip_mark, buffer_fail_rc=81, clip_fail_rc=82, stem="smartgis_m2_hook"
):
    """Buffer then clip. Each mark is written only after that write-back opens."""
    folder = tempfile.gettempdir()
    src = os.path.join(folder, stem + "_in.geojson")
    buffered = os.path.join(folder, stem + "_buffer.geojson")
    clip = os.path.join(folder, stem + "_clip.geojson")
    clipped = os.path.join(folder, stem + "_clip_out.geojson")
    write_text(src, _SQUARE)
    write_text(clip, _CLIP)
    sg.run_processing(
        id="native.buffer",
        args=json.dumps(
            {
                "input": slash(src),
                "output": slash(buffered),
                "distance": 0.5,
            }
        ),
    )
    open_path(sg, buffered, fail_rc=buffer_fail_rc)
    require_count(sg, 1, "buffer write-back")
    sg.mark(buffer_mark)
    sg.run_processing(
        id="native.clip",
        args=json.dumps(
            {
                "input": slash(src),
                "output": slash(clipped),
                "clip": slash(clip),
            }
        ),
    )
    open_path(sg, clipped, fail_rc=clip_fail_rc)
    require_count(sg, 1, "clip write-back")
    sg.mark(clip_mark)


def milestones(sg):
    buffer_and_clip(sg, "m2-buffer-ok", "m2-clip-ok")
    sg.mark("pass")
    sg.mark("detached")


def main():
    import glang.plugin as gp

    milestones(gp)
