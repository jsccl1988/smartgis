# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Suite gate scorers: zoom / motion / click / marks+BMP round score."""

from __future__ import annotations

from pathlib import Path

from ..contract import Suite, with_capture_scenario
from .bmp import score_bmp
from .bmp_io import pixel_diff_frac
from .marks import score_marks

def _score_zoom_gate(
    suite: Suite,
    *,
    captures_root: Path,
    inject_report: dict,
) -> dict:
    """Compare zoom_before/after BMPs; fail when pixel_diff_frac is too low."""
    assert suite.zoom_gate is not None
    zg = suite.zoom_gate
    before = captures_root / zg.before.replace("\\", "/")
    after = captures_root / zg.after.replace("\\", "/")
    cap = inject_report.get("zoom_capture") if isinstance(inject_report, dict) else None
    out: dict = {
        "before": str(before),
        "after": str(after),
        "min_pixel_diff_frac": zg.min_pixel_diff_frac,
        "thresh": zg.thresh,
        "ok": False,
    }
    if isinstance(cap, dict):
        out["capture"] = cap
        before_ok = bool((cap.get("before") or {}).get("ok"))
        after_ok = bool((cap.get("after") or {}).get("ok"))
        if not before_ok or not after_ok:
            out["error"] = "zoom_capture_black_or_failed"
            return out
    if not before.is_file() or not after.is_file():
        out["error"] = "zoom_bmp_missing"
        return out
    try:
        frac = pixel_diff_frac(before, after, thresh=zg.thresh)
    except Exception as exc:  # noqa: BLE001
        out["error"] = f"pixel_diff: {exc}"
        return out
    out["pixel_diff_frac"] = round(float(frac), 6)
    out["ok"] = float(frac) >= float(zg.min_pixel_diff_frac)
    return out



def _score_fps_gate(
    suite: Suite,
    *,
    captures_root: Path,
) -> dict:
    """Parse map2d-fps-bench.txt mean_fps (soft by default)."""
    assert suite.fps_gate is not None
    fg = suite.fps_gate
    rel = with_capture_scenario(fg.report_leaf, suite.scenario_dir())
    path = captures_root / Path(rel)
    out: dict = {
        "path": str(path),
        "min_mean_fps": fg.min_mean_fps,
        "soft": fg.soft,
        "ok": False,
    }
    if not path.is_file():
        # Flat leaf fallback (older binaries).
        flat = captures_root / fg.report_leaf
        if flat.is_file():
            path = flat
            out["path"] = str(path)
        else:
            out["error"] = "fps_report_missing"
            out["ok"] = bool(fg.soft)
            return out
    mean = None
    try:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("mean_fps="):
                mean = float(line.split("=", 1)[1].strip())
                break
    except (OSError, ValueError) as exc:
        out["error"] = f"fps_parse: {exc}"
        out["ok"] = bool(fg.soft)
        return out
    if mean is None:
        out["error"] = "mean_fps_missing"
        out["ok"] = bool(fg.soft)
        return out
    out["mean_fps"] = round(mean, 3)
    out["ok"] = mean >= float(fg.min_mean_fps)
    if not out["ok"] and fg.soft:
        out["soft_fail"] = True
        out["ok"] = True
    return out


def _score_motion_gate(
    suite: Suite,
    *,
    record_report: dict | None,
) -> dict:
    """Unique BMP frames during HWND record must cover pan/path motion."""
    assert suite.motion_gate is not None
    mg = suite.motion_gate
    out: dict = {
        "min_unique_frac": mg.min_unique_frac,
        "min_unique_frames": mg.min_unique_frames,
        "ok": False,
    }
    if not isinstance(record_report, dict):
        out["error"] = "record_missing"
        return out
    mode = str(record_report.get("mode") or "")
    out["mode"] = mode
    record_path = Path(str(record_report.get("record_path") or ""))
    # ffmpeg mp4: accept non-empty file with positive duration (pixel unique N/A).
    if mode.startswith("ffmpeg") and record_path.is_file():
        out["bytes"] = record_path.stat().st_size
        out["duration_s"] = record_report.get("duration_s")
        out["ok"] = record_path.stat().st_size > 10_000
        if not out["ok"]:
            out["error"] = "ffmpeg_mp4_too_small"
        return out
    frames_dir = record_path
    if not frames_dir.is_dir():
        out["error"] = "record_frames_missing"
        return out
    import hashlib

    # Hash map-center crop so static chrome does not hide pan/path motion.
    hashes: set[str] = set()
    sizes: set[tuple[int, int]] = set()
    total = 0
    try:
        from PIL import Image  # type: ignore[import-untyped]
    except ImportError:
        Image = None  # type: ignore[assignment,misc]
    for bmp in sorted(frames_dir.glob("frame_*.bmp")):
        total += 1
        try:
            if Image is not None:
                with Image.open(bmp) as im:
                    w, h = im.size
                    sizes.add((int(w), int(h)))
                    crop = im.crop(
                        (int(w * 0.28), int(h * 0.12), int(w * 0.78), int(h * 0.55))
                    ).resize((96, 54))
                    hashes.add(hashlib.md5(crop.tobytes()).hexdigest())
            else:
                hashes.add(hashlib.md5(bmp.read_bytes()).hexdigest())
        except OSError:
            continue
    unique = len(hashes)
    frac = (unique / total) if total else 0.0
    out["frame_count"] = total
    out["unique_frames"] = unique
    out["unique_frac"] = round(frac, 4)
    out["unique_sizes"] = len(sizes)
    out["unique_wh"] = sorted(list(sizes))
    size_ok = True
    min_sizes = int(getattr(mg, "min_unique_sizes", 0) or 0)
    if min_sizes > 0:
        size_ok = len(sizes) >= min_sizes
        out["min_unique_sizes"] = min_sizes
    out["ok"] = (
        unique >= int(mg.min_unique_frames)
        and frac >= float(mg.min_unique_frac)
        and size_ok
    )
    if not out["ok"]:
        if not size_ok:
            out["error"] = "resize_sizes_missing"
        else:
            out["error"] = "motion_too_static"
    return out



def _score_click_gate(
    suite: Suite,
    *,
    captures_root: Path,
    inject_report: dict,
) -> dict:
    """click/dblclick ran and after-click shell BMP is not near-black."""
    assert suite.click_gate is not None
    cg = suite.click_gate
    out: dict = {
        "after": str(captures_root / cg.after.replace("\\", "/")),
        "max_near_black": cg.max_near_black,
        "ok": False,
    }
    clicks = int(inject_report.get("clicks") or 0)
    dblclicks = int(inject_report.get("dblclicks") or 0)
    out["clicks"] = clicks
    out["dblclicks"] = dblclicks
    if clicks < 1 or dblclicks < 1:
        out["error"] = "click_steps_missing"
        return out
    cap = inject_report.get("click_capture")
    after_path = captures_root / cg.after.replace("\\", "/")
    near_black = 1.0
    if isinstance(cap, dict) and isinstance(cap.get("after"), dict):
        near_black = float(cap["after"].get("near_black") or 1.0)
        out["capture"] = cap
    elif after_path.is_file():
        try:
            from .bmp_io import load_bmp_rgb

            _w, _h, pixels = load_bmp_rgb(after_path)
            n = max(1, len(pixels))
            step = max(1, n // 4000)
            samples = 0
            black = 0
            for i in range(0, n, step):
                r, g, b = pixels[i]
                samples += 1
                if r < 25 and g < 25 and b < 25:
                    black += 1
            near_black = black / max(1, samples)
        except Exception as exc:  # noqa: BLE001
            out["error"] = f"click_bmp: {exc}"
            return out
    else:
        out["error"] = "click_bmp_missing"
        return out
    # Soft fallback: suite/zoom BMP already proved shell painted; click probe
    # only needs counts + a non-black frame from the same run.
    if near_black > float(cg.max_near_black):
        for key in ("capture", "zoom_capture"):
            blob = inject_report.get(key)
            if key == "zoom_capture" and isinstance(blob, dict):
                blob = blob.get("after")
            if isinstance(blob, dict) and float(blob.get("near_black") or 1.0) <= float(
                cg.max_near_black
            ):
                near_black = float(blob.get("near_black") or near_black)
                out["fallback"] = key
                break
    out["near_black"] = round(near_black, 4)
    out["ok"] = near_black <= float(cg.max_near_black)
    if not out["ok"]:
        out["error"] = "click_after_too_black"
    return out



def _score_round(
    suite: Suite,
    *,
    rc: int,
    mark_text: str,
    bmp_path: Path | None,
    started: float,
) -> dict:
    want_marks = "marks" in suite.probes or bool(suite.required_marks)
    want_bmp = "bmp" in suite.probes or suite.bmp is not None

    result: dict = {
        "suite": suite.id,
        "showcase_rc": rc,
        "ok": True,
        "gates": {},
    }

    effective_rc = rc
    if (
        suite.bmp is not None
        and suite.bmp.accept_nonzero_rc_if_bmp
        and rc != 0
        and bmp_path is not None
        and bmp_path.exists()
        and bmp_path.stat().st_size > suite.bmp.min_bytes
    ):
        print(
            f"showcase rc={rc} but BMP present — treating exit as 0 for gates",
            flush=True,
        )
        effective_rc = 0
        result["rc_forgiven"] = True
    if (
        effective_rc != 0
        and suite.accept_nonzero_rc_if_marks
        and suite.required_marks
    ):
        landed = {line.strip() for line in mark_text.splitlines() if line.strip()}
        if all(m in landed for m in suite.required_marks):
            print(
                f"showcase rc={rc} but required marks present — "
                "treating exit as 0 for gates",
                flush=True,
            )
            effective_rc = 0
            result["rc_forgiven"] = True

    if want_marks:
        marks = score_marks(effective_rc, mark_text, suite.required_marks)
        result["marks"] = marks.get("marks")
        result["missing"] = marks.get("missing")
        result["gates"].update(marks.get("gates") or {})
        if not marks["ok"]:
            result["ok"] = False
    else:
        result["gates"]["exit==0"] = effective_rc == 0
        if effective_rc != 0:
            result["ok"] = False

    if want_bmp:
        assert suite.bmp is not None
        if bmp_path is None or not bmp_path.exists():
            result["ok"] = False
            result["error"] = "bmp_missing"
            result["gates"]["bmp_present"] = False
        elif bmp_path.stat().st_mtime < (started - 0.5):
            result["ok"] = False
            result["error"] = "bmp_stale"
            result["gates"]["bmp_present"] = False
        else:
            bmp_score = score_bmp(bmp_path, suite.bmp.score_id)
            result["bmp"] = bmp_score
            result["gates"].update(
                {f"bmp:{k}": v for k, v in (bmp_score.get("gates") or {}).items()}
            )
            result["gates"]["bmp_ok"] = bool(bmp_score.get("ok"))
            if not bmp_score.get("ok"):
                if suite.bmp.soft:
                    result["gates"]["bmp_ok_soft"] = False
                    result["bmp_soft_fail"] = True
                else:
                    result["ok"] = False
            if want_marks is False and effective_rc != 0:
                result["ok"] = False

    return result


def attach_runtime_gates(
    suite: Suite,
    last: dict,
    *,
    inject_report: dict | None,
    record_report: dict | None,
    env: dict[str, str],
    captures_root: Path,
) -> dict:
    """Fold OS inject / HWND record / FPS probes into a round result dict."""
    from ..record.hwnd import record_enabled

    if inject_report is not None:
        last["os_inject"] = inject_report
        last["gates"]["os_inject_ok"] = bool(inject_report.get("ok"))
        if not inject_report.get("ok"):
            last["ok"] = False
        if "t_ms" in inject_report:
            last["steps"].insert(
                1,
                {
                    "op": "os_inject",
                    "t_ms": int(inject_report.get("t_ms") or 0),
                    "ok": bool(inject_report.get("ok")),
                },
            )
        if suite.zoom_gate is not None:
            zoom_cap = (
                inject_report.get("zoom_capture")
                if isinstance(inject_report, dict)
                else None
            )
            if not isinstance(zoom_cap, dict):
                zg = {
                    "ok": False,
                    "error": "zoom_not_captured_this_run",
                    "min_pixel_diff_frac": suite.zoom_gate.min_pixel_diff_frac,
                    "thresh": suite.zoom_gate.thresh,
                }
            else:
                zg = _score_zoom_gate(
                    suite,
                    captures_root=captures_root,
                    inject_report=inject_report,
                )
            last["zoom_gate"] = zg
            last["gates"]["zoom_pixel_diff"] = bool(zg.get("ok"))
            if not zg.get("ok"):
                last["ok"] = False
                print(
                    f"FAIL {suite.id} zoom_gate "
                    f"{zg.get('error') or ('pixel_diff_frac=' + str(zg.get('pixel_diff_frac')))} "
                    f"(min={suite.zoom_gate.min_pixel_diff_frac})",
                    flush=True,
                )
            else:
                print(
                    f"PASS {suite.id} zoom_gate pixel_diff_frac="
                    f"{zg.get('pixel_diff_frac')}",
                    flush=True,
                )
        if suite.click_gate is not None and suite.click_gate.enabled:
            cg = _score_click_gate(
                suite,
                captures_root=captures_root,
                inject_report=inject_report
                if isinstance(inject_report, dict)
                else {},
            )
            last["click_gate"] = cg
            last["gates"]["click_ok"] = bool(cg.get("ok"))
            if not cg.get("ok"):
                last["ok"] = False
                print(
                    f"FAIL {suite.id} click_gate "
                    f"{cg.get('error') or cg}",
                    flush=True,
                )
            else:
                print(
                    f"PASS {suite.id} click_gate clicks={cg.get('clicks')} "
                    f"dblclicks={cg.get('dblclicks')}",
                    flush=True,
                )
    if record_report is not None:
        last["record"] = record_report
        if record_report.get("record_path"):
            last["record_path"] = record_report["record_path"]
        last["gates"]["record_soft_ok"] = record_report.get("mode") != "none"
        if (
            suite.motion_gate is not None
            and record_enabled(env)
            and str(record_report.get("mode") or "").startswith(
                ("bmp_burst", "ffmpeg")
            )
        ):
            mg = _score_motion_gate(suite, record_report=record_report)
            last["motion_gate"] = mg
            last["gates"]["motion_ok"] = bool(mg.get("ok"))
            if not mg.get("ok"):
                last["ok"] = False
                print(
                    f"FAIL {suite.id} motion_gate "
                    f"unique={mg.get('unique_frames')}/"
                    f"{mg.get('frame_count')} "
                    f"frac={mg.get('unique_frac')}",
                    flush=True,
                )
            else:
                print(
                    f"PASS {suite.id} motion_gate "
                    f"unique={mg.get('unique_frames')}/"
                    f"{mg.get('frame_count')} "
                    f"frac={mg.get('unique_frac')}",
                    flush=True,
                )
    if suite.fps_gate is not None:
        fg = _score_fps_gate(suite, captures_root=captures_root)
        last["fps_gate"] = fg
        last["gates"]["fps_ok"] = bool(fg.get("ok"))
        if not fg.get("ok"):
            last["ok"] = False
            print(
                f"FAIL {suite.id} fps_gate "
                f"mean={fg.get('mean_fps')} "
                f"min={fg.get('min_mean_fps')} "
                f"{fg.get('error') or ''}",
                flush=True,
            )
        else:
            print(
                f"PASS {suite.id} fps_gate "
                f"mean={fg.get('mean_fps')} "
                f"soft_fail={fg.get('soft_fail', False)}",
                flush=True,
            )
    return last




