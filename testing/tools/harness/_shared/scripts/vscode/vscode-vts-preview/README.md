<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS VTS Preview (VS Code / Cursor)

In-repo extension: open ASCII VTK `StructuredGrid` (`*.vts`) from
`orthogrid3d` in a Three.js webview (orbit + wireframe + nodes).

## Install

```bat
testing\tools\harness\_shared\scripts\vscode\install_vscode_vts_preview.bat
```

Then **Developer: Reload Window**.

## Use

1. Open e.g. `out/Debug/captures/plugin/plugin-showcase-orthogrid3d.vts`
2. Command Palette → **SmartGIS: Preview VTS**
3. Or right-click the `.vts` in the explorer → **SmartGIS: Preview VTS**

Drag to orbit, wheel to zoom. HUD shows `nx×ny×nz`. Optional
`orthogonality` cell data tints node colors.

## Scope (M0)

- ASCII VTK XML `StructuredGrid` only (matches `write_vtk_structured`)
- Not product Scene3D; not binary VTK / UnstructuredGrid
- Vendored `three@0.128` + `OrbitControls` under `media/`

## Layout

| Path | Role |
| --- | --- |
| `package.json` | Extension manifest |
| `extension.js` | Parse `.vts`, open webview |
| `media/preview.js` | Three.js scene |
| `media/three.min.js` | Vendored Three.js |
| `media/OrbitControls.js` | Vendored OrbitControls |
