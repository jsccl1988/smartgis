<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# MapLibre out of `src/gpu` Implementation Plan

**Status:** cancelled  
**Date:** 2026-09-27  
**Cancelled by:** User decision — **删除 maplibre，将来再考虑**. Do not migrate to `src/render/maplibre`; remove the Native pin from the product tree instead.  
**Spec:** [`../specs/2026-09-27-maplibre-out-of-gpu-design.md`](../specs/2026-09-27-maplibre-out-of-gpu-design.md) (superseded；archived sibling)

## Outcome

Tasks that would have created `src/render/maplibre/` and moved `smt_enable_maplibre` are **not** to be executed. Replacement work: delete gpu probe + GN flag + third_party `maplibre_native` target; keep tile wire aliases; document deferred in `third_party/maplibre/README.md`.

Historical checkbox plan body is obsolete; do not resume move-to-render tasks.
