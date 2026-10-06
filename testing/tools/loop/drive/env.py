# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Suite env: parent-matrix scrub + driver/script defaults."""

from __future__ import annotations

from ..contract import Suite
from .process import merge_env

_SCRUB_KEYS = (
    "FORCE_GDI_MAP_OVERLAY",
    "FORCE_CONTENT_MAPVIEW_2D",
    "PREFER_FLYCUBE_2D",
    "PREFER_GDI_DEVICE",
    "MAP2D_SHOWCASE_GPU",
    "MAP2D_EXPORT_REUSE",
    "MAP2D_FPS_BENCH_MS",
    "MAP2D_ENGINE",
    "MAP2D_NO_HILLSHADE",
    # Parent matrix shells leave scenic / world3d bare / wireframe on;
    # that yields red-ball HWND BMPs and flat PERF_BARE disks for product
    # suites that expect FlyCube + full materials / GDI carto.
    "SCENE3D_ENGINE",
    "SCENE3D_WIREFRAME",
    "PLUGIN_WORLD3D_PERF_BARE",
)


def prepare_env(suite: Suite) -> dict[str, str]:
    env = merge_env(suite.env)
    env["HARNESS_SUITE"] = suite.id
    for key in _SCRUB_KEYS:
        env.pop(key, None)
    for key, value in suite.env.items():
        env[str(key)] = str(value)
    # browser.world3d.* / ui.scene need FlyCube present pixels.
    # browser.map2d.browse (2D stress) uses ContentMapView + GDI overlay —
    # do not force FlyCube or shell BitBlt records a black clip-children hole.
    if suite.id == "ui.scene" or suite.id.startswith("browser.world3d."):
        env["FORCE_CONTENT_MAPVIEW_2D"] = "0"
        env["PREFER_FLYCUBE_2D"] = "1"
        env["FORCE_GDI_MAP_OVERLAY"] = "0"
        env["SCENE3D_ENGINE"] = "flycube"
    elif suite.id == "browser.map2d.browse":
        env["FORCE_CONTENT_MAPVIEW_2D"] = "1"
        env["PREFER_FLYCUBE_2D"] = "0"
        env["FORCE_GDI_MAP_OVERLAY"] = "1"
    # Suite JSON is SoT after parent-matrix scrub + id defaults. ui.interact
    # pins MAP2D_ENGINE=vista / SCENE3D_ENGINE=flycube — do not clobber.
    for key, value in suite.env.items():
        env[str(key)] = str(value)
    # PE entry (host) vs gesture body — must not share one env key.
    script = suite.script_path()
    if script is not None and script.is_file():
        env["UI_INTERACT_SCRIPT"] = str(script.resolve())
    gesture = suite.gesture_script_path()
    if gesture is not None and gesture.is_file():
        env["UI_INTERACT_GESTURE_SCRIPT"] = str(gesture.resolve())
    if suite.driver == "os":
        env["UI_INTERACT_DRIVER"] = "os"
        env.setdefault("UI_INTERACT_OS_WAIT_MS", "8000")
        env.setdefault("UI_SHOWCASE_LINGER_MS", "0")
    return env
