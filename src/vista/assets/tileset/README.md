<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 3D Tiles streaming (`vista/assets/tileset`)

CPU-side **`tileset.json`** parse, frustum/SSE tile selection, LRU content cache, and glTF-family decode for tile payloads.

## Content decode matrix

Tile bytes are decoded by **`decode_gltf_bytes`** (`tileset_decode.cc`) / public **`ModelAsset::decode_content`**. Standalone Assimp **`load_file`** does not open tile wrappers.

| Format | Decode | On failure |
| --- | --- | --- |
| **`.glb` / glTF binary** | tinygltf → meshes | `ensure_tileset_content` → **`put_failed(uri)`** (1-byte stub, no retry storm) |
| **`.gltf`** (JSON) | tinygltf ASCII | same |
| **`.b3dm`** | Skip 3D Tiles header → embedded glTF/GLB | same |
| **`.i3dm`** | **Not supported** (returns false immediately) | **`put_failed`** |
| **`.pnts`** | **Not supported** | **`put_failed`** |
| **`.cmpt`** | **Not supported** | **`put_failed`** |

Requires **`HAS_TINYGLTF`** at build time; without it, all decodes fail closed → `put_failed`.

## Selection: geometric error / SSE / caps

**`select_tiles`** walks the tree while  
`geometric_error * sse_denominator / eye_distance > max_sse`  
and refines into children (`Refine::kReplace` drops parent; `kAdd` keeps parent + children).

| API | Role |
| --- | --- |
| `select_tiles_limited(..., max_sse, max_tiles)` | SSE refine, then truncate visible list to **`max_tiles`** (0 = unlimited) |
| `ensure_tileset_content(..., max_ensure)` | Resolve at most **`max_ensure`** cache misses per call (0 = all selected misses) |

**Product defaults** (present pump, not in this module): `content::TilesetStreamSession` in  
`src/content/browser/present/scene3d/frame/tileset/tileset_stream.h`:

| Parameter | Default | Meaning |
| --- | --- | --- |
| `kDefaultMaxSse` | **0** | Refine while tile SSE **> 0** (positive `geometricError` → prefer deeper tiles when close) |
| `kDefaultMaxTiles` | **16** | Hard cap on selected tiles per pump |
| `kDefaultMaxEnsure` | **2** | At most two new URI decodes per frame |
| `kDefaultCacheBytes` | **8 MiB** | LRU resident decoded payload budget |

`World::stream_tileset` exposes the same SSE/tile limits for tests without the content session wrapper.

## Verify

```bat
build.bat debug tileset_test
out\Debug\tileset_test.exe
```
