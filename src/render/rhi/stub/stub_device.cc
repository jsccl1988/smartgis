// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

#include <windows.h>

// Merged Null / GDI / GL leftover HWND adapters. One StubDevice tagged by
// Backend; not a real GDI/GL rasterizer (see rhi/ subdirectory split plan).

namespace render {
namespace rhi {
namespace {

// Stub device for kNull / kGdi / kGl. Present and destroy policy differ by tag.
class StubDevice : public Device {
 public:
  explicit StubDevice(Backend tag) : tag_(tag) {}

  bool initialize(const DeviceDesc& desc) override {
    if (tag_ == Backend::kNull) {
      return true;
    }
    hwnd_ = static_cast<HWND>(desc.native_window);
    return hwnd_ != nullptr;
  }

  void shutdown() override {
    if (tag_ != Backend::kNull) {
      hwnd_ = nullptr;
    }
  }

  void present() override {
    if (tag_ == Backend::kGdi && hwnd_) {
      InvalidateRect(hwnd_, nullptr, FALSE);
    }
    // kNull / kGl: no-op present (GL leftover adapter has empty present).
  }

  Backend backend() const override { return tag_; }

  uint32_t execute_count() const override {
    return tag_ == Backend::kNull ? execute_calls_ : 0;
  }

  // Explicit Null/GDI/GL: no shared depth SRV (fog uses CameraCB far-ray).
  Texture* shared_depth_texture() override { return nullptr; }

  CommandList* create_command_list() override { return new StubCommandList(); }

  void destroy_command_list(CommandList* list) override {
    if (tag_ == Backend::kNull) {
      // Intentionally leak stub objects. The same process links FlyCube;
      // repeated operator delete of stub CommandList/Buffer/Texture has hung
      // headless CI (scene_gpu_test TIMEOUT, leftover_record_test flaky
      // finish). Production backends (FlyCube/GDI) still free their resources.
      (void)list;
      return;
    }
    delete list;
  }

  bool execute(CommandList* list) override {
    auto* stub = static_cast<StubCommandList*>(list);
    if (stub == nullptr || !stub->closed) {
      return false;
    }
    if (tag_ == Backend::kNull) {
      ++execute_calls_;
    }
    return true;
  }

  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override {
    return detail::make_stub_buffer(byte_size, usage);
  }

  void destroy_buffer(Buffer* buffer) override {
    if (tag_ == Backend::kNull) {
      (void)buffer;
      return;
    }
    detail::destroy_stub_buffer(buffer);
  }

  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override {
    return detail::upload_stub_buffer(buffer, data, byte_size);
  }

  void destroy_texture(Texture* texture) override {
    if (tag_ == Backend::kNull) {
      (void)texture;
      return;
    }
    Device::destroy_texture(texture);
  }

 private:
  Backend tag_;
  HWND hwnd_ = nullptr;
  uint32_t execute_calls_ = 0;
};

}  // namespace

Device* create_null_device() {
  return new StubDevice(Backend::kNull);
}

Device* create_gdi_device() {
  return new StubDevice(Backend::kGdi);
}

Device* create_gl_device() {
  return new StubDevice(Backend::kGl);
}

}  // namespace rhi
}  // namespace render
