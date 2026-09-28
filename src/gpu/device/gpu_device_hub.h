// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_DEVICE_GPU_DEVICE_HUB_H_
#define GPU_DEVICE_GPU_DEVICE_HUB_H_

#include "gpu/device/adapter_id.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace render {
namespace rhi {
class Device;
class Texture;
}
namespace graph {
class Effect;
}
}  // namespace render

namespace gpu {
namespace detail {

class OutputSurface;

// Process-wide hub: one GPU process owns N adapter device slots.
// Surfaces bind to an AdapterId; composers and RHI devices are per slot.
class GpuDeviceHub {
 public:
  static GpuDeviceHub& instance();
  ~GpuDeviceHub();

  std::vector<AdapterInfo> enumerate_adapters() const;

  AdapterId primary_adapter() const { return kAdapterPrimary; }

  // Best-effort monitor affinity; falls back to primary.
  AdapterId prefer_adapter_for_monitor(void* hmonitor) const;

  bool bind_surface(OutputSurface* surface, AdapterId adapter);
  void unbind_surface(OutputSurface* surface);
  AdapterId adapter_of(const OutputSurface* surface) const;

  // Re-pin |surface| to the adapter preferred for |hmonitor| (M5).
  bool rebind_surface_to_monitor(OutputSurface* surface, void* hmonitor);

  bool is_software_fallback(AdapterId adapter) const;
  void set_software_fallback(AdapterId adapter, bool sticky);

  // Creates/reuses a render::rhi::Device for |adapter| (Dx12 preferred, Null
  // fallback). Returns null when sticky software fallback is set.
  render::rhi::Device* ensure_rhi_device(AdapterId adapter);

  // Opaque accessor (tests / legacy). Prefer ensure_rhi_device.
  void* rhi_device_for(AdapterId adapter) const;
  void set_rhi_device_for(AdapterId adapter, void* device);

  // M5: destroy RHI resources for |adapter|, sticky software, bump surfaces.
  void notify_device_lost(AdapterId adapter);

  // Optional underlay Effects (non-owning). Recorded before overlay quads.
  // graph / scene types stay out of OutputSurface; shell never sees these.
  void set_underlay_effects(AdapterId adapter,
                            std::vector<render::graph::Effect*> effects);
  std::vector<render::graph::Effect*> underlay_effects(AdapterId adapter) const;

  // Generation bumped on device-lost / rebind so texture caches invalidate.
  uint32_t slot_generation(AdapterId adapter) const;

  // M3: per-adapter RHI texture cache (survives per-frame composers).
  render::rhi::Texture* find_cached_texture(AdapterId adapter,
                                            uint64_t key) const;
  void put_cached_texture(AdapterId adapter, uint64_t key,
                          render::rhi::Texture* texture);
  void clear_texture_cache(AdapterId adapter);

 private:
  struct DeviceSlot {
    AdapterId id = kAdapterInvalid;
    bool software_fallback = false;
    std::unique_ptr<render::rhi::Device> owned_rhi;
    void* external_rhi = nullptr;  // set_rhi_device_for override
    std::vector<render::graph::Effect*> underlay;
    uint32_t generation = 1;
    std::unordered_map<uint64_t, render::rhi::Texture*> texture_cache;
  };

  DeviceSlot& slot_for(AdapterId adapter);
  const DeviceSlot* find_slot(AdapterId adapter) const;

  mutable std::mutex mu_;
  std::unordered_map<AdapterId, DeviceSlot> slots_;
  std::unordered_map<const OutputSurface*, AdapterId> surface_adapters_;
};

GpuDeviceHub& device_hub();

}  // namespace detail
}  // namespace gpu

#endif  // GPU_DEVICE_GPU_DEVICE_HUB_H_
