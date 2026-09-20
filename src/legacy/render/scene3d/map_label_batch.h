// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_MAP_LABEL_BATCH_H_
#define SMT_LEGACY_RENDER_SCENE3D_MAP_LABEL_BATCH_H_

#include "legacy/render/scene3d/bl3d_object.h"

#if !defined(SCENE3D_EXPORT_DEFINED)
#define SCENE3D_EXPORT_DEFINED
#if defined(SCENE3D_EXPORTS)
#define SCENE3D_EXPORT_API __declspec(dllexport)
#define SCENE3D_EXPORT_CLASS __declspec(dllexport)
#else
#define SCENE3D_EXPORT_API __declspec(dllimport)
#define SCENE3D_EXPORT_CLASS __declspec(dllimport)
#endif
#endif

#include <string>
#include <vector>

namespace render {

struct MapLabel {
  std::string text;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  int priority = 5;
};

// Screen-space 3D labels: GDI+ AA textures + halo, with collision declutter.
class SCENE3D_EXPORT_CLASS MapLabelBatch : public Smt3DObject {
 public:
  MapLabelBatch();
  ~MapLabelBatch() override;

  long Init(Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "") override;
  long Create(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) override;
  long Render(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Destroy() override;

  void add_label(const MapLabel& label);
  void clear_labels();
  int label_count() const { return static_cast<int>(labels_.size()); }

 private:
  bool ensure_font(LP3DRENDERDEVICE device);

  std::vector<MapLabel> labels_;
  uint font_id_ = 0;
  bool font_ready_ = false;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_MAP_LABEL_BATCH_H_
