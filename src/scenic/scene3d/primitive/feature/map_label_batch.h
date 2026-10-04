// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_SCENE3D_MAP_LABEL_BATCH_H_
#define SCENIC_SCENE3D_MAP_LABEL_BATCH_H_

#include <deque>
#include <string>
#include <vector>

#include "scenic/scenic_impl_export.h"
#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {

struct MapLabel {
  std::string text;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  int priority = 5;
};

// Screen-space 3D labels: GDI+ AA textures + halo, with collision declutter.
class LEGACY_RENDER_EXPORT MapLabelBatch : public Object3d {
 public:
  MapLabelBatch();
  ~MapLabelBatch() override;

  long Init(::base::Vector3& vPos, Material& matMaterial,
            const char* szTexName = "") override;
  long Create(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) override;
  long Render(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Destroy() override;

  bool prefers_immediate_context() const override { return true; }

  void add_label(const MapLabel& label);
  void clear_labels();
  int label_count() const { return static_cast<int>(labels_.size()); }

 private:
  bool ensure_font(LP3DRENDERDEVICE device);

  // Per unique text|priority raster (+ optional GL tex id).
  // deque: bgra.data() stays stable so D3D DrawScreenBgra pointer-cache hits.
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
  std::deque<RasterCache> raster_cache_;
  // Anti-flicker: hold last screen pixels + declutter winners across Presents.
  std::vector<int> last_sx_;
  std::vector<int> last_sy_;
  std::vector<int> sticky_keep_;
  uint font_id_ = 0;
  bool font_ready_ = false;
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_SCENE3D_MAP_LABEL_BATCH_H_
