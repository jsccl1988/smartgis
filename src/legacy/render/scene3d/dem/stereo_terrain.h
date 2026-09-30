// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_STEREO_TERRAIN_H_
#define SMT_LEGACY_RENDER_SCENE3D_STEREO_TERRAIN_H_

#include <memory>

#include "legacy/render/legacy_render_export.h"
#include "legacy/render/rhi3d/public/resource/index_buffer.h"
#include "legacy/render/rhi3d/public/resource/vertex_buffer.h"
#include "legacy/render/scene3d/dem/dem_height_field.h"
#include "legacy/render/scene3d/scene/object.h"

namespace render {

// Coarse DEM grid mesh for leftover 3D (hypsometric color + normals).
class LEGACY_RENDER_EXPORT StereoTerrain : public Smt3DObject {
 public:
  StereoTerrain();
  ~StereoTerrain() override;

  long Init(Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "") override;
  long Create(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) override;
  long Render(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Destroy() override;

  // Non-owning borrow (caller keeps |field| alive for the terrain lifetime).
  void set_height_field(const DemHeightField* field);
  // Takes ownership of |field|; deleted with this terrain (scene owns terrain).
  void adopt_height_field(DemHeightField* field);
  const DemHeightField* height_field() const { return field_; }

 private:
  std::unique_ptr<DemHeightField> owned_field_;
  const DemHeightField* field_ = nullptr;
  SmtVertexBuffer* vb_ = nullptr;
  SmtIndexBuffer* ib_ = nullptr;
  ulong index_count_ = 0;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_STEREO_TERRAIN_H_
