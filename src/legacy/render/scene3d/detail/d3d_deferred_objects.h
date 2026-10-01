// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_SCENE3D_DETAIL_D3D_DEFERRED_OBJECTS_H_
#define LEGACY_RENDER_SCENE3D_DETAIL_D3D_DEFERRED_OBJECTS_H_

#include <vector>

#include "legacy/render/rhi3d/impl/common/frame/prep_runner.h"
#include "legacy/render/rhi3d/impl/d3d/ext/ext_interface.h"
#include "legacy/render/scene3d/scene/object.h"

namespace render {
namespace detail {

// P3: partition visible objects across D3D11 deferred contexts.
// Returns false → caller must serial Render. GL always returns false.
inline bool render_objects_d3d_deferred(LP3DRENDERDEVICE device,
                                        const std::vector<Smt3DObject*>& objs) {
  if (!device || objs.empty() || !smt_d3d_deferred_enabled()) {
    return false;
  }
  const int workers = rhi3d_prep_worker_count();
  if (workers <= 1) {
    return false;
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
    for (size_t i = slot; i < objs.size(); i += static_cast<size_t>(workers)) {
      if (objs[i]) {
        objs[i]->Render(device);
      }
    }
    (void)call_smt_d3d_bind_deferred(device, -1);
  });
  return call_smt_d3d_finish_deferred(device) == SMT_ERR_NONE;
}

}  // namespace detail
}  // namespace render

#endif  // LEGACY_RENDER_SCENE3D_DETAIL_D3D_DEFERRED_OBJECTS_H_
