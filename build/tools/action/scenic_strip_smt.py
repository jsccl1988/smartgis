#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Strip Smt* type prefixes and swap thin legacy includes under src/scenic."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCENIC = ROOT / "src" / "scenic"

# Longest-first type renames (scenic-owned).
TYPE_MAP: list[tuple[str, str]] = [
    ("SmtMultitextureFuncImpl", "MultitextureFuncImpl"),
    ("SmtD3DGPUStateManager", "D3dGpuStateManager"),
    ("SmtGLGPUStateManager", "GlGpuStateManager"),
    ("SmtMultitextureFunc", "MultitextureFunc"),
    ("SmtRhi2dRenderDevice", "Rhi2dRenderDevice"),
    ("SmtShadersFuncImpl", "ShadersFuncImpl"),
    ("SmtSceneOctTree", "SceneOctTree"),
    ("Smt3DPointCloud", "PointCloud3d"),
    ("SmtVertex3DList", "Vertex3dList"),
    ("SmtAlphaTestState", "AlphaTestState"),
    ("SmtDepthTestState", "DepthTestState"),
    ("SmtD3DDeviceCaps", "D3dDeviceCaps"),
    ("SmtD3DIndexBuffer", "D3dIndexBuffer"),
    ("SmtD3DRenderDevice", "D3dRenderDevice"),
    ("SmtD3DVertexBuffer", "D3dVertexBuffer"),
    ("SmtGLDeviceCaps", "GlDeviceCaps"),
    ("SmtGLIndexBuffer", "GlIndexBuffer"),
    ("SmtGLRenderDevice", "GlRenderDevice"),
    ("SmtGLVertexBuffer", "GlVertexBuffer"),
    ("SmtMipmapFuncImpl", "MipmapFuncImpl"),
    ("SmtProgramManager", "ProgramManager"),
    ("SmtRenderContext", "RenderContext"),
    ("SmtShaderManager", "ShaderManager"),
    ("SmtSurfaceObject", "SurfaceObject"),
    ("SmtTextureManager", "TextureManager"),
    ("SmtVertexOctTree", "VertexOctTree"),
    ("Smt3DDeviceCaps", "DeviceCaps3d"),
    ("Smt3DRenderDevice", "RenderDevice3d"),
    ("Smt3DRenderable", "Renderable3d"),
    ("SmtFBOFuncImpl", "FboFuncImpl"),
    ("SmtGPUStateManager", "GpuStateManager"),
    ("SmtVSyncFuncImpl", "VSyncFuncImpl"),
    ("Smt3DObject", "Object3d"),
    ("Smt3DMovable", "Movable3d"),
    ("Smt3DRenderer", "Renderer3d"),
    ("SmtArbvCamera", "ArbvCamera"),
    ("SmtBlendState", "BlendState"),
    ("SmtFrameBuffer", "FrameBuffer"),
    ("SmtGeoObject", "GeoObject"),
    ("SmtMatrixState", "MatrixState"),
    ("SmtMipmapFunc", "MipmapFunc"),
    ("SmtNorthArray", "NorthArray"),
    ("SmtOrthCamera", "OrthCamera"),
    ("SmtPerspCamera", "PerspCamera"),
    ("SmtRenderBuffer", "RenderBuffer"),
    ("SmtRenderDevice", "RenderDevice2d"),
    ("SmtShadersFunc", "ShadersFunc"),
    ("SmtVertexBuffer", "VertexBuffer"),
    ("SmtVideoBuffer", "VideoBuffer"),
    ("SmtVBOFuncImpl", "VboFuncImpl"),
    ("SmtFPSCamera", "FpsCamera"),
    ("SmtIndexBuffer", "IndexBuffer"),
    ("SmtVertex3D", "Vertex3d"),
    ("SmtFBOFunc", "FboFunc"),
    ("SmtGPUState", "GpuState"),
    ("SmtMaterial", "Material"),
    ("SmtProgram", "Program"),
    ("SmtRenderer", "Renderer2d"),
    ("SmtTexture", "Texture"),
    ("SmtVBOFunc", "VboFunc"),
    ("SmtVSyncFunc", "VSyncFunc"),
    ("SmtCamera", "Camera"),
    ("SmtShader", "Shader"),
    ("SmtScenes", "Scenes"),
    ("SmtScene", "Scene"),
    ("SmtSphere", "Sphere"),
    ("SmtTerrain", "Terrain"),
    ("SmtColor", "Color"),
    ("SmtGLText", "GlText"),
    ("SmtLight", "Light"),
    ("SmtWater", "Water"),
    ("SmtCube", "Cube"),
    ("SmtCombinedCamera", "CombinedCamera"),
    ("SmtD3DBeginDeferredDrawFn", "D3dBeginDeferredDrawFn"),
    ("SmtD3DBindDeferredWorkerFn", "D3dBindDeferredWorkerFn"),
    ("SmtD3DFinishDeferredDrawFn", "D3dFinishDeferredDrawFn"),
    ("SmtD3DDeferredEnabledFn", "D3dDeferredEnabledFn"),
    ("SmtD3DCaptureBgr24Fn", "D3dCaptureBgr24Fn"),
    ("SmtD3DDrawScreenBgraFn", "D3dDrawScreenBgraFn"),
    ("SmtD3DBeginDeferredDraw", "D3dBeginDeferredDraw"),
    ("SmtD3DBindDeferredWorker", "D3dBindDeferredWorker"),
    ("SmtD3DFinishDeferredDraw", "D3dFinishDeferredDraw"),
    ("SmtD3DDeferredEnabled", "D3dDeferredEnabled"),
    ("SmtD3DCaptureBgr24", "D3dCaptureBgr24"),
    ("SmtD3DDrawScreenBgra", "D3dDrawScreenBgra"),
    ("SmtRhi2dClearBgraSubmit", "Rhi2dClearBgraSubmit"),
    ("SmtRhi2dSetBgraSubmit", "Rhi2dSetBgraSubmit"),
    ("SmtRhi2dBgraSubmitFn", "Rhi2dBgraSubmitFn"),
    ("SmtGdiPlusRenderDevice", "GdiPlusRenderDevice"),
    ("SmtGdiSimpleRenderDevice", "GdiSimpleRenderDevice"),
    ("SmtGdiRenderDevice", "GdiRenderDevice"),
    ("SmtSkiaRenderDevice", "SkiaRenderDevice"),
    ("Smt_3DBase", "Base3d"),
    # Do NOT rename leftover carto types still included (SmtStyle, SmtBrushDesc…).
]

INCLUDE_SWAPS: list[tuple[str, str]] = [
    (
        '#include "legacy/core/macros/macros.h"',
        '#include "scenic/detail/err.h"',
    ),
    (
        '#include "legacy/core/types/types.h"',
        '#include "scenic/detail/geom.h"',
    ),
    (
        '#include "legacy/gis/present/carto/style_bas_struct.h"',
        '#include "scenic/detail/viewport.h"',
    ),
]

TEXT_EXTS = {".h", ".hh", ".hpp", ".c", ".cc", ".cpp", ".cxx", ".inl", ".md", ".gn"}


def rewrite(text: str) -> str:
    for old, new in INCLUDE_SWAPS:
        text = text.replace(old, new)
    for old, new in TYPE_MAP:
        text = re.sub(rf"\b{re.escape(old)}\b", new, text)
    # Guard / comment cleanups
    text = text.replace("SMT_LEGACY_RENDER_", "SCENIC_")
    text = text.replace("LEGACY_RENDER_SCENE3D_", "SCENIC_SCENE3D_")
    text = text.replace("LEGACY_RENDER_RHI2D_", "SCENIC_RHI2D_")
    text = text.replace("LEGACY_RENDER_RHI3D_", "SCENIC_RHI3D_")
    text = text.replace("SMT_SCENIC_DETAIL_MATH_ALIAS_H_", "SCENIC_DETAIL_MATH_ALIAS_H_")
    return text


def main() -> int:
    dry = "--dry-run" in sys.argv
    changed = 0
    for path in sorted(SCENIC.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in TEXT_EXTS:
            continue
        if "detail/err.h" in path.as_posix() or "detail/geom.h" in path.as_posix():
            continue
        if "detail/viewport.h" in path.as_posix():
            continue
        if path.name == "scenic_strip_smt.py":
            continue
        try:
            raw = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            raw = path.read_bytes().decode("utf-8", errors="replace")
        out = rewrite(raw)
        if out != raw:
            changed += 1
            rel = path.relative_to(ROOT).as_posix()
            print(("DRY " if dry else "OK  ") + rel)
            if not dry:
                path.write_bytes(out.encode("utf-8"))
    print(f"files_changed={changed} dry={dry}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
