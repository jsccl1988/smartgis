<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-world3d-opt — reference

## Matrix model

```
同等渲染物料
        │
        ├─ FlyCube/DX12  (SmartGisViews --plugin-showcase=world3d)
        │     SMT_SCENE3D_ENGINE=flycube
        │     parallel ∈ {prep_default, prep_0, prep_on}
        │
        ├─ Null / GDI    (same Views entry; GPU-off / prefer GDI)
        │
        ├─ Stereo/GL     (SmartGis --scene3d-showcase china)
        │     SMT_SCENE3D_ENGINE=stereo_gl
        │     SMT_STEREO_API=OpenGL
        │
        └─ Leftover/D3D11 (SmartGis --scene3d-showcase china)
              SMT_SCENE3D_ENGINE=stereo_d3d
              SMT_STEREO_API=Direct3D
```

## Fairness

| Claim | OK? |
| --- | --- |
| Compare FlyCube prep on vs off on wall / phases | Yes |
| Require BMP for gl / d3d_leftover (not N/A) | Yes |
| Treat leftover wall_ms as equal GPU work to FlyCube warm | **No** |
| Fabricate BMP when SmartGis.exe missing | **No** — build then re-run |

## SMT_SCENE3D_ENGINE

Implemented in `content::apply_scene3d_engine_from_env()`:

| Value | Engine | Also sets |
| --- | --- | --- |
| `flycube` / `dx12` | `kFlyCube` | — |
| `stereo_gl` / `opengl` / `gl` | `kStereoGl` | `SMT_STEREO_API=OpenGL` |
| `stereo_d3d` / `d3d` / `direct3d` / `d3d11` | `kStereoGl` | `SMT_STEREO_API=Direct3D` |
| `gdi` | `kGdi` | — |

When set, `browser_main` does **not** force showcase GDI default.

Helpers: `prefer_scene3d_stereo_opengl()` · `prefer_scene3d_stereo_d3d()`.

## Leftover BMP leaves

| Backend | Preferred leaf |
| --- | --- |
| OpenGL | `legacy-scene3d-showcase-china-gl.bmp` (fallback `…-china.bmp`) |
| D3D11 | `legacy-scene3d-showcase-china-d3d.bmp` |

## Artifact layout

```
out/Debug/captures/analysis/world3d_opt/matrix/
  flycube/ … gl/ … d3d_leftover/ …
  world3d_backend_matrix.csv
  world3d_backend_matrix.json
  MATRIX.md
```
