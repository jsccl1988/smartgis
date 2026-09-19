// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_SCENE3D_STEREO_TERRAIN_H_
#define SMT_LEGACY_RENDER_SCENE3D_STEREO_TERRAIN_H_

#include "legacy/render/scene3d/bl3d_object.h"
#include "legacy/render/scene3d/dem_height_field.h"

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

namespace render {

// Coarse DEM grid mesh for leftover 3D (hypsometric color + normals).
class SCENE3D_EXPORT_CLASS StereoTerrain : public Smt3DObject {
 public:
  StereoTerrain();
  ~StereoTerrain() override;

  long Init(Vector3& vPos, SmtMaterial& matMaterial,
            const char* szTexName = "") override;
  long Create(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Update(LP3DRENDERDEVICE p3DRenderDevice, float fElapsed) override;
  long Render(LP3DRENDERDEVICE p3DRenderDevice) override;
  long Destroy() override;

  void set_height_field(const DemHeightField* field) { field_ = field; }
  const DemHeightField* height_field() const { return field_; }

 private:
  const DemHeightField* field_ = nullptr;
  SmtVertexBuffer* vb_ = nullptr;
  SmtIndexBuffer* ib_ = nullptr;
  ulong index_count_ = 0;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_SCENE3D_STEREO_TERRAIN_H_
