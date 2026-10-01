// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_MAP_LABEL_BATCH_H_
#define SMT_LEGACY_RENDER_SCENE3D_MAP_LABEL_BATCH_H_

#include <string>
#include <vector>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/scene3d/scene/object.h"

namespace render {

struct MapLabel {
  std::string text;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  int priority = 5;
};

// Screen-space 3D labels: GDI+ AA textures + halo, with collision declutter.
class LEGACY_RENDER_EXPORT MapLabelBatch : public Smt3DObject {
 public:
  MapLabelBatch();
  ~MapLabelBatch() override;

  long Init(::base::Vector3& vPos, SmtMaterial& matMaterial,
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

  // Per unique text|priority raster (+ optional GL tex id).
  // Vector (not unordered_map): hash-bucket vectors of debug iterators hit
  // MSVC xmemory aligned-delete asserts under this Debug DLL CRT mix.
  struct RasterCache {
    std::string key;
    std::vector<unsigned char> bgra;
    int w = 0;
    int h = 0;
    unsigned gl_tex = 0;  // GLuint; 0 = not uploaded
  };

  RasterCache* find_raster(const std::string& key);
  RasterCache* insert_raster(RasterCache&& entry);
  void clear_raster_cache();

  std::vector<MapLabel> labels_;
  std::vector<RasterCache> raster_cache_;
  uint font_id_ = 0;
  bool font_ready_ = false;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_MAP_LABEL_BATCH_H_
