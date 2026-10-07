// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Device-thread terrain upload, solid-tint gate, and kTerrain record.
// WorldPass owns sync_from / record_draws and composes this type.

#ifndef VISTA_PASS_WORLD_TERRAIN_PASS_H_
#define VISTA_PASS_WORLD_TERRAIN_PASS_H_

#include <cstdint>
#include <vector>

#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "vista/vista_export.h"
#include "vista/component/world/space/cull/frustum_aabb.h"
#include "vista/component/world/instance.h"
#include "vista/mesh/tessellate.h"
#include "vista/pass/world/gpu_mesh.h"

namespace vista {

// Uploads DEM / TIN TerrainPayload meshes, owns the atmosphere solid-terrain
// gate, and records NodeKind::kTerrain draws. Raster- and TIN-derived payloads
// share this path.
class VISTA_EXPORT TerrainPass {
 public:
  TerrainPass();
  ~TerrainPass() = default;

  TerrainPass(const TerrainPass&) = delete;
  TerrainPass& operator=(const TerrainPass&) = delete;

  // Sky-on / stereo gate: force solid land tint from hypso bake mean.
  void update_solid_terrain(const std::vector<Instance>& instances,
                            uint64_t generation, bool gate);
  bool solid_terrain_forced() const { return solid_terrain_forced_; }
  void clear_solid_terrain_cache();

  // RGB to apply when solid_terrain_forced (identity when gate off).
  void forced_solid_rgb(float* r, float* g, float* b) const;

  // Upload albedo texture + configure mesh tint for one kTerrain instance.
  // |cpu| may gain has_image when a texture uploads. Returns false only on
  // Device upload failure after a texture was required.
  bool prepare_mesh(render::rhi::Device* device, const Instance& inst,
                    TessMesh* cpu, GpuMesh* mesh, float solid_r, float solid_g,
                    float solid_b, float solid_a);

  // Record all kTerrain GpuMeshes (same path for raster and TIN payloads).
  void record(render::rhi::CommandList* list,
              const render::rhi::RenderPassDesc& pass, uint32_t width,
              uint32_t height, const std::vector<GpuMesh>& meshes,
              bool* pass_opened, render::rhi::Pipeline* solid,
              render::rhi::Pipeline* textured,
              render::rhi::Pipeline* lit_pipeline,
              render::rhi::Pipeline* lit_textured_pipeline,
              const render::programs::Light& light,
              const FrustumPlanes* cull_frustum,
              const std::vector<uint8_t>* mesh_visible);

 private:
  bool solid_terrain_forced_ = false;
  bool solid_terrain_cached_ = false;
  uint64_t solid_terrain_cache_gen_ = 0;
  float solid_terrain_rgb_[3] = {1.f, 1.f, 1.f};
};

}  // namespace vista

#endif  // VISTA_PASS_WORLD_TERRAIN_PASS_H_
