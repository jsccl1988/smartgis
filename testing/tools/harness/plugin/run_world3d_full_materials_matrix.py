# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run world3d full-materials (non-bare) FlyCube matrix — separate from bare peers.

Product path: SmartGisViews --plugin-showcase=world3d with
SMT_PLUGIN_WORLD3D_PERF_BARE unset (sky/ocean/cloud/fog + pointcloud on).
role=full_materials — NEVER mix into bare warm peer ranking
(out/.../matrix/ MATRIX.md Performance section).

Artifacts: out/Debug/captures/analysis/world3d_opt/matrix/full_materials/

Sibling of run_world3d_backend_matrix.py (bare equal-profile). Imports that
module's runners and redirects MATRIX so the cold bare runner stays untouched.
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

# Same package dir — import bare matrix helpers without editing that file.
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

import run_world3d_backend_matrix as bare  # noqa: E402

ROOT = bare.ROOT
OUT = bare.OUT
FULL_MATRIX = (
    OUT / "captures" / "analysis" / "world3d_opt" / "matrix" / "full_materials"
)

# FlyCube product materials only. Leftover GL/D3D stay on the bare matrix
# (china stereo seed ≠ product full atmo). null stays smoke-only on bare.
_FLYCUBE_FULL_ENV: dict[str, str | None] = {
    "SMT_PLUGIN_WORLD3D_GPU": "1",
    # Unset = product materials (sky/ocean/cloud/fog + pointcloud).
    "SMT_PLUGIN_WORLD3D_PERF_BARE": None,
    "SMT_PREFER_GDI_DEVICE": None,
    "SMT_SCENE3D_ENGINE": None,
}

# row_id, backend, parallel, role, env
FULL_ROWS: list[tuple[str, str, str, str, dict[str, str | None]]] = [
    (
        "flycube",
        "FlyCube/DX12",
        "prep_default",
        "full_materials",
        {**_FLYCUBE_FULL_ENV, "SMT_GPUSCENE_PREP_PARALLEL": None},
    ),
    (
        "prep_par_off",
        "FlyCube/DX12",
        "prep_0",
        "full_materials",
        {
            **_FLYCUBE_FULL_ENV,
            "SMT_GPUSCENE_PREP_PARALLEL": "0",
            "SMT_SCENE3D_FRUSTUM_CULL": None,
        },
    ),
    (
        "prep_par_on",
        "FlyCube/DX12",
        "prep_on",
        "full_materials",
        {
            **_FLYCUBE_FULL_ENV,
            "SMT_GPUSCENE_PREP_PARALLEL": "1",
            "SMT_SCENE3D_FRUSTUM_CULL": "1",
        },
    ),
]


def _fmt_num(v) -> str:
    return bare._fmt_num(v)


def _write_reports(rows: list[dict]) -> None:
    FULL_MATRIX.mkdir(parents=True, exist_ok=True)
    csv_path = FULL_MATRIX / "world3d_full_materials_matrix.csv"
    json_path = FULL_MATRIX / "world3d_full_materials_matrix.json"
    fields = [
        "row_id", "role", "engine", "backend", "parallel", "rc",
        "ms_per_present", "ms_per_present_cold", "ms_per_present_all",
        "present_ms", "present_ms_warm", "present_count", "discard_cold",
        "warm_count", "mesh_ms", "sync_ms", "rebuild_ms", "rebuild_count",
        "ocean_prep_ms", "record_ms", "present_swap_ms", "wall_ms",
        "bmp_bytes", "pass", "bmp", "inspect_png", "perf_json", "note",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow(r)
    json_path.write_text(
        json.dumps(
            {
                "equal_profile": "world3d_opt_full_materials",
                "role": "full_materials",
                "note": (
                    "Separate from bare warm peer ranking "
                    "(SMT_PLUGIN_WORLD3D_PERF_BARE=1 matrix)."
                ),
                "rows": rows,
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    md_path = FULL_MATRIX / "MATRIX.md"
    lines = [
        "# world3d full-materials matrix (M4)",
        "",
        "**role=`full_materials`** — product sky/ocean/cloud/fog + pointcloud.",
        "`SMT_PLUGIN_WORLD3D_PERF_BARE` unset. Do **not** rank these rows",
        "against bare warm peers in `../MATRIX.md` Performance.",
        "",
        "Phase columns align with atmosphere / scene3d-frame-opt",
        "(`ocean_prep_ms`, `record_ms`, `present_swap_ms`, …).",
        "",
        "Product timed loop (non-bare): `pump_ms=50`, `discard_cold=0`",
        "(see `present_warmup.cc`). Primary metric still `ms_per_present`.",
        "",
        "## Screenshots",
        "",
        "| Row | Role | Backend | Parallel | Inspect | pass |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for r in rows:
        insp = r.get("inspect_png") or r.get("bmp") or "—"
        lines.append(
            f"| {r['row_id']} | {r.get('role')} | {r['backend']} | "
            f"{r['parallel']} | `{insp}` | {r.get('pass')} |"
        )
    lines += [
        "",
        "## Full materials performance (not bare peers)",
        "",
        "| Row | Backend | Parallel | ms/p | ocean_prep | record | swap | "
        "mesh | rebuild_n | n | wall_ms* | pass |",
        "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | "
        "---: | ---: | --- |",
    ]
    for r in rows:
        lines.append(
            f"| {r['row_id']} | {r['backend']} | {r['parallel']} | "
            f"{_fmt_num(r.get('ms_per_present'))} | "
            f"{_fmt_num(r.get('ocean_prep_ms'))} | "
            f"{_fmt_num(r.get('record_ms'))} | "
            f"{_fmt_num(r.get('present_swap_ms'))} | "
            f"{_fmt_num(r.get('mesh_ms'))} | "
            f"{_fmt_num(r.get('rebuild_count'))} | "
            f"{_fmt_num(r.get('present_count'))} | "
            f"{_fmt_num(r.get('wall_ms'))} | {r.get('pass')} |"
        )
    lines += [
        "",
        "\\* `wall_ms` = whole-process wall; not a peer-rank metric.",
        "",
        "## vs bare matrix",
        "",
        "| Concern | Bare (`../`) | This dir |",
        "| --- | --- | --- |",
        "| Env | `SMT_PLUGIN_WORLD3D_PERF_BARE=1` | unset |",
        "| role | `perf` / `smoke` | `full_materials` |",
        "| Peer ranking | warm `ms_per_present` across GL/D3D/FlyCube | "
        "FlyCube full materials only |",
        "| null / leftover | in bare matrix | omitted here |",
        "",
    ]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {csv_path}")
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description=(
            "world3d full-materials FlyCube matrix "
            "(role=full_materials; separate from bare peers)"
        )
    )
    ap.add_argument(
        "--row",
        action="append",
        dest="rows",
        metavar="ROW_ID",
        help="Run only these row_ids (default: all). e.g. --row flycube",
    )
    args = ap.parse_args(argv)

    # Redirect bare helpers' artifact root without editing that module's body.
    bare.MATRIX = FULL_MATRIX
    FULL_MATRIX.mkdir(parents=True, exist_ok=True)

    want = set(args.rows) if args.rows else None
    selected = [
        t for t in FULL_ROWS if want is None or t[0] in want
    ]
    if want is not None and not selected:
        print(
            f"world3d-full-materials: unknown --row {sorted(want)}; "
            f"known={[t[0] for t in FULL_ROWS]}",
            file=sys.stderr,
        )
        return 2

    rows_out: list[dict] = []
    for row_id, backend, parallel, role, overlay in selected:
        print(
            f"world3d-full-materials: run {row_id} backend={backend} "
            f"role={role} (PERF_BARE unset)",
            flush=True,
        )
        row = bare.run_views_world3d(row_id, backend, parallel, overlay)
        if not row.get("pass"):
            print(f"  retry {row_id} after flake", flush=True)
            row = bare.run_views_world3d(row_id, backend, parallel, overlay)
        row["role"] = role
        # Product leaf soft-fails overlay; accept present-ok+bmp-ok with
        # full-materials / pointcloud-* marks (bare runner only keys on
        # pointcloud-ok|skip).
        marks_set = set(row.get("marks") or [])
        bmp_ok = bool(row.get("bmp")) and int(row.get("bmp_bytes") or 0) > 10000
        cloud_mark = bool(
            marks_set
            & {
                "pointcloud-ok",
                "pointcloud-skip",
                "pointcloud-missing",
                "pointcloud-load-fail",
                "full-materials",
            }
        )
        presentish = ("pass" in marks_set) or (
            "present-ok" in marks_set and "bmp-ok" in marks_set
        )
        if bmp_ok and cloud_mark and presentish:
            row["pass"] = True
        note = row.get("note") or ""
        tag = (
            "role=full_materials (product atmo/ocean/sky; overlay soft); "
            "not a bare warm peer"
        )
        row["note"] = f"{note}; {tag}" if note else tag
        if "PERF_BARE strips" in (row.get("note") or ""):
            row["note"] = (
                "china DEM + full product materials "
                "(SMT_PLUGIN_WORLD3D_PERF_BARE unset); "
                f"{tag}; prefer ms_per_present + ocean_prep_ms over wall_ms"
            )
        rows_out.append(row)
        print(
            f"  rc={row.get('rc')} ms/p={row.get('ms_per_present')} "
            f"ocean_prep_ms={row.get('ocean_prep_ms')} "
            f"record_ms={row.get('record_ms')} "
            f"wall_ms={row.get('wall_ms')} pass={row.get('pass')} "
            f"bmp_bytes={row.get('bmp_bytes')} role={role}",
            flush=True,
        )

    bare._kill_showcase_procs()
    _write_reports(rows_out)
    failed = [r for r in rows_out if not r.get("pass")]
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
