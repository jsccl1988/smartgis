// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_SCENE3D_SCENE_D3D_DEFERRED_OBJECTS_H_
#define LEGACY_RENDER_SCENE3D_SCENE_D3D_DEFERRED_OBJECTS_H_

#include <vector>

#include "legacy/render/rhi3d/impl/common/frame/prep_runner.h"
#include "legacy/render/rhi3d/impl/d3d/ext/ext_interface.h"
#include "legacy/render/scene3d/primitive/feature/map_label_batch.h"
#include "legacy/render/scene3d/scene/object.h"

namespace render {
namespace detail {

// P3: partition visible objects across D3D11 deferred contexts.
// Screen-space objects (MapLabelBatch) stay on the immediate context.
// When true, *out_immediate holds labels to draw after the caller re-applies
// the camera (deferred workers may race Push/Pop on modelview_).
// Returns false → caller must serial Render. GL always returns false.
inline bool render_objects_d3d_deferred(
    LP3DRENDERDEVICE device, const std::vector<Smt3DObject*>& objs,
    std::vector<Smt3DObject*>* out_immediate = nullptr) {
  if (out_immediate) {
    out_immediate->clear();
  }
  if (!device || objs.empty() || !smt_d3d_deferred_enabled()) {
    return false;
  }
  const int workers = rhi3d_prep_worker_count();
  if (workers <= 1) {
    return false;
  }

  std::vector<Smt3DObject*> deferred;
  std::vector<Smt3DObject*> immediate;
  deferred.reserve(objs.size());
  immediate.reserve(4);
  for (Smt3DObject* obj : objs) {
    if (!obj) {
      continue;
    }
    // RTTI only: derived vtables historically left prefers_immediate_context
    // null (SOFTWARE_NX_FAULT). MapLabelBatch is the sole immediate specialist.
    if (dynamic_cast<MapLabelBatch*>(obj) != nullptr) {
      immediate.push_back(obj);
    } else {
      deferred.push_back(obj);
    }
  }

  if (deferred.empty()) {
    if (out_immediate) {
      *out_immediate = std::move(immediate);
    } else {
      for (Smt3DObject* obj : immediate) {
        obj->Render(device);
      }
    }
    return true;
  }

  if (call_smt_d3d_begin_deferred(device, workers) != SMT_ERR_NONE) {
    return false;
  }
  Rhi3dPrepRunner& prep = rhi3d_shared_prep_runner();
  prep.ensure_workers(workers);
  prep.run_jobs(static_cast<size_t>(workers), [&](size_t slot) {
    if (call_smt_d3d_bind_deferred(device, static_cast<int>(slot)) !=
        SMT_ERR_NONE) {
      return;
    }
    for (size_t i = slot; i < deferred.size();
         i += static_cast<size_t>(workers)) {
      if (deferred[i]) {
        deferred[i]->Render(device);
      }
    }
    (void)call_smt_d3d_bind_deferred(device, -1);
  });
  if (call_smt_d3d_finish_deferred(device) != SMT_ERR_NONE) {
    return false;
  }
  if (out_immediate) {
    *out_immediate = std::move(immediate);
  } else {
    for (Smt3DObject* obj : immediate) {
      obj->Render(device);
    }
  }
  return true;
}

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_SCENE3D_SCENE_D3D_DEFERRED_OBJECTS_H_
