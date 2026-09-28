// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/device/gpu_device_hub.h"

#include "gpu/display/output_surface.h"
#include "render/rhi/rhi.h"

#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <dxgi.h>
#include <dxgi1_2.h>

namespace gpu {
namespace detail {
namespace {

void copy_desc(char* dst, size_t dst_bytes, const WCHAR* src) {
  if (!dst || dst_bytes == 0) {
    return;
  }
  dst[0] = '\0';
  if (!src) {
    return;
  }
  WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, static_cast<int>(dst_bytes),
                      nullptr, nullptr);
  dst[dst_bytes - 1] = '\0';
}

std::vector<AdapterInfo> enumerate_dxgi_adapters() {
  std::vector<AdapterInfo> out;
  IDXGIFactory1* factory = nullptr;
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                reinterpret_cast<void**>(&factory))) ||
      !factory) {
    return out;
  }

  for (UINT i = 0;; ++i) {
    IDXGIAdapter1* adapter = nullptr;
    if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (!adapter) {
      continue;
    }
    DXGI_ADAPTER_DESC1 desc = {};
    if (FAILED(adapter->GetDesc1(&desc))) {
      adapter->Release();
      continue;
    }
    const bool software =
        (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
    AdapterInfo info;
    info.id = static_cast<AdapterId>(i);
    info.is_hardware = !software;
    info.luid = (static_cast<uint64_t>(desc.AdapterLuid.HighPart) << 32) |
                static_cast<uint64_t>(desc.AdapterLuid.LowPart);
    copy_desc(info.description, sizeof(info.description), desc.Description);
    out.push_back(info);
    adapter->Release();
  }
  factory->Release();
  return out;
}

std::unique_ptr<render::rhi::Device> try_create_rhi_device(AdapterId adapter) {
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::create_device;
  using render::rhi::preferred_gpu_backend;

  auto try_backend =
      [adapter](Backend backend) -> std::unique_ptr<render::rhi::Device> {
    std::unique_ptr<render::rhi::Device> device(create_device(backend));
    if (!device) {
      return nullptr;
    }
    DeviceDesc desc;
    desc.native_window = nullptr;
    desc.width = 1;
    desc.height = 1;
    desc.adapter_index = (adapter == kAdapterInvalid) ? 0u : adapter;
    if (!device->initialize(desc)) {
      device->shutdown();
      return nullptr;
    }
    return device;
  };

  std::unique_ptr<render::rhi::Device> device =
      try_backend(preferred_gpu_backend());
  if (device) {
    return device;
  }
  return try_backend(Backend::kNull);
}

}  // namespace

GpuDeviceHub& GpuDeviceHub::instance() {
  static GpuDeviceHub hub;
  return hub;
}

GpuDeviceHub::~GpuDeviceHub() = default;

GpuDeviceHub& device_hub() {
  return GpuDeviceHub::instance();
}

std::vector<AdapterInfo> GpuDeviceHub::enumerate_adapters() const {
  std::vector<AdapterInfo> listed = enumerate_dxgi_adapters();
  if (listed.empty()) {
    AdapterInfo primary;
    primary.id = kAdapterPrimary;
    primary.is_hardware = true;
    primary.luid = 0;
    std::strncpy(primary.description, "primary",
                 sizeof(primary.description) - 1);
    return {primary};
  }
  std::vector<AdapterInfo> hardware;
  for (const AdapterInfo& a : listed) {
    if (a.is_hardware) {
      hardware.push_back(a);
    }
  }
  return hardware.empty() ? listed : hardware;
}

AdapterId GpuDeviceHub::adapter_for_luid(uint64_t luid) const {
  if (luid == 0) {
    return primary_adapter();
  }
  for (const AdapterInfo& info : enumerate_adapters()) {
    if (info.luid == luid) {
      return info.id;
    }
  }
  return primary_adapter();
}

AdapterId GpuDeviceHub::prefer_adapter_for_monitor(void* hmonitor) const {
  if (!hmonitor) {
    return primary_adapter();
  }
  const HMONITOR monitor = static_cast<HMONITOR>(hmonitor);
  IDXGIFactory1* factory = nullptr;
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                reinterpret_cast<void**>(&factory))) ||
      !factory) {
    return primary_adapter();
  }

  AdapterId found = kAdapterInvalid;
  for (UINT i = 0; found == kAdapterInvalid; ++i) {
    IDXGIAdapter1* adapter = nullptr;
    if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (!adapter) {
      continue;
    }
    for (UINT oi = 0; found == kAdapterInvalid; ++oi) {
      IDXGIOutput* output = nullptr;
      if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (!output) {
        continue;
      }
      DXGI_OUTPUT_DESC od = {};
      if (SUCCEEDED(output->GetDesc(&od)) && od.Monitor == monitor) {
        found = static_cast<AdapterId>(i);
      }
      output->Release();
    }
    adapter->Release();
  }
  factory->Release();
  return found == kAdapterInvalid ? primary_adapter() : found;
}

GpuDeviceHub::DeviceSlot& GpuDeviceHub::slot_for(AdapterId adapter) {
  auto it = slots_.find(adapter);
  if (it == slots_.end()) {
    DeviceSlot slot;
    slot.id = adapter;
    it = slots_.emplace(adapter, std::move(slot)).first;
  }
  return it->second;
}

const GpuDeviceHub::DeviceSlot* GpuDeviceHub::find_slot(
    AdapterId adapter) const {
  const auto it = slots_.find(adapter);
  return it == slots_.end() ? nullptr : &it->second;
}

bool GpuDeviceHub::bind_surface(OutputSurface* surface, AdapterId adapter) {
  if (!surface || adapter == kAdapterInvalid) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  (void)slot_for(adapter);
  surface_adapters_[surface] = adapter;
  surface->set_adapter_id(adapter);
  return true;
}

void GpuDeviceHub::unbind_surface(OutputSurface* surface) {
  if (!surface) {
    return;
  }
  std::lock_guard<std::mutex> lock(mu_);
  surface_adapters_.erase(surface);
  surface->set_adapter_id(kAdapterInvalid);
}

AdapterId GpuDeviceHub::adapter_of(const OutputSurface* surface) const {
  if (!surface) {
    return kAdapterInvalid;
  }
  std::lock_guard<std::mutex> lock(mu_);
  const auto it = surface_adapters_.find(surface);
  if (it != surface_adapters_.end()) {
    return it->second;
  }
  const AdapterId pinned = surface->adapter_id();
  return pinned == kAdapterInvalid ? kAdapterInvalid : pinned;
}

bool GpuDeviceHub::rebind_surface_to_monitor(OutputSurface* surface,
                                             void* hmonitor) {
  if (!surface) {
    return false;
  }
  const AdapterId preferred = prefer_adapter_for_monitor(hmonitor);
  std::lock_guard<std::mutex> lock(mu_);
  const AdapterId previous = [&]() {
    const auto it = surface_adapters_.find(surface);
    return it == surface_adapters_.end() ? surface->adapter_id() : it->second;
  }();
  DeviceSlot& slot = slot_for(preferred);
  if (previous != preferred && previous != kAdapterInvalid) {
    ++slot.generation;
  }
  surface_adapters_[surface] = preferred;
  surface->set_adapter_id(preferred);
  return true;
}

bool GpuDeviceHub::is_software_fallback(AdapterId adapter) const {
  std::lock_guard<std::mutex> lock(mu_);
  const DeviceSlot* slot = find_slot(adapter);
  return slot && slot->software_fallback;
}

void GpuDeviceHub::set_software_fallback(AdapterId adapter, bool sticky) {
  std::lock_guard<std::mutex> lock(mu_);
  slot_for(adapter).software_fallback = sticky;
}

render::rhi::Device* GpuDeviceHub::ensure_rhi_device(AdapterId adapter) {
  std::lock_guard<std::mutex> lock(mu_);
  DeviceSlot& slot = slot_for(adapter);
  if (slot.software_fallback) {
    return nullptr;
  }
  if (slot.external_rhi) {
    return static_cast<render::rhi::Device*>(slot.external_rhi);
  }
  if (slot.owned_rhi) {
    return slot.owned_rhi.get();
  }
  std::unique_ptr<render::rhi::Device> created = try_create_rhi_device(adapter);
  if (!created) {
    slot.software_fallback = true;
    return nullptr;
  }
  slot.owned_rhi = std::move(created);
  return slot.owned_rhi.get();
}

void* GpuDeviceHub::rhi_device_for(AdapterId adapter) const {
  std::lock_guard<std::mutex> lock(mu_);
  const DeviceSlot* slot = find_slot(adapter);
  if (!slot) {
    return nullptr;
  }
  if (slot->external_rhi) {
    return slot->external_rhi;
  }
  return slot->owned_rhi.get();
}

void GpuDeviceHub::set_rhi_device_for(AdapterId adapter, void* device) {
  std::lock_guard<std::mutex> lock(mu_);
  DeviceSlot& slot = slot_for(adapter);
  slot.external_rhi = device;
  if (device) {
    slot.software_fallback = false;
  }
}

void GpuDeviceHub::notify_device_lost(AdapterId adapter) {
  std::lock_guard<std::mutex> lock(mu_);
  DeviceSlot& slot = slot_for(adapter);
  render::rhi::Device* device = slot.owned_rhi.get();
  if (!device && slot.external_rhi) {
    device = static_cast<render::rhi::Device*>(slot.external_rhi);
  }
  for (auto& entry : slot.texture_cache) {
    if (device) {
      device->destroy_texture(entry.second);
    }
  }
  slot.texture_cache.clear();
  if (slot.owned_rhi) {
    slot.owned_rhi->shutdown();
    slot.owned_rhi.reset();
  }
  slot.external_rhi = nullptr;
  slot.software_fallback = true;
  ++slot.generation;
  for (auto& entry : surface_adapters_) {
    if (entry.second == adapter && entry.first) {
      const_cast<OutputSurface*>(entry.first)->bump_generation();
    }
  }
}

void GpuDeviceHub::set_underlay_effects(
    AdapterId adapter, std::vector<render::graph::Effect*> effects) {
  std::lock_guard<std::mutex> lock(mu_);
  slot_for(adapter).underlay = std::move(effects);
}

std::vector<render::graph::Effect*> GpuDeviceHub::underlay_effects(
    AdapterId adapter) const {
  std::lock_guard<std::mutex> lock(mu_);
  const DeviceSlot* slot = find_slot(adapter);
  return slot ? slot->underlay : std::vector<render::graph::Effect*>{};
}

uint32_t GpuDeviceHub::slot_generation(AdapterId adapter) const {
  std::lock_guard<std::mutex> lock(mu_);
  const DeviceSlot* slot = find_slot(adapter);
  return slot ? slot->generation : 0;
}

render::rhi::Texture* GpuDeviceHub::find_cached_texture(AdapterId adapter,
                                                        uint64_t key) const {
  if (key == 0) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(mu_);
  const DeviceSlot* slot = find_slot(adapter);
  if (!slot) {
    return nullptr;
  }
  const auto it = slot->texture_cache.find(key);
  return it == slot->texture_cache.end() ? nullptr : it->second;
}

void GpuDeviceHub::put_cached_texture(AdapterId adapter, uint64_t key,
                                      render::rhi::Texture* texture) {
  if (key == 0 || !texture) {
    return;
  }
  std::lock_guard<std::mutex> lock(mu_);
  DeviceSlot& slot = slot_for(adapter);
  auto it = slot.texture_cache.find(key);
  if (it != slot.texture_cache.end()) {
    render::rhi::Device* device = slot.owned_rhi.get();
    if (!device && slot.external_rhi) {
      device = static_cast<render::rhi::Device*>(slot.external_rhi);
    }
    if (device && it->second && it->second != texture) {
      device->destroy_texture(it->second);
    }
    it->second = texture;
    return;
  }
  slot.texture_cache.emplace(key, texture);
}

void GpuDeviceHub::clear_texture_cache(AdapterId adapter) {
  std::lock_guard<std::mutex> lock(mu_);
  DeviceSlot& slot = slot_for(adapter);
  render::rhi::Device* device = slot.owned_rhi.get();
  if (!device && slot.external_rhi) {
    device = static_cast<render::rhi::Device*>(slot.external_rhi);
  }
  for (auto& entry : slot.texture_cache) {
    if (device) {
      device->destroy_texture(entry.second);
    }
  }
  slot.texture_cache.clear();
}

}  // namespace detail
}  // namespace gpu
