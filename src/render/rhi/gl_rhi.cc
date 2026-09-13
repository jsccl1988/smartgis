// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

#include <windows.h>

namespace render {
namespace rhi {
namespace {

class GlDevice : public Device {
 public:
  bool initialize(const DeviceDesc& desc) override {
    hwnd_ = static_cast<HWND>(desc.native_window);
    return hwnd_ != nullptr;
  }
  void shutdown() override { hwnd_ = nullptr; }
  void present() override {}
  Backend backend() const override { return Backend::kGl; }

  CommandList* create_command_list() override { return new StubCommandList(); }
  void destroy_command_list(CommandList* list) override { delete list; }
  bool execute(CommandList* list) override {
    auto* stub = static_cast<StubCommandList*>(list);
    return stub != nullptr && stub->closed;
  }
  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override {
    return detail::make_stub_buffer(byte_size, usage);
  }
  void destroy_buffer(Buffer* buffer) override {
    detail::destroy_stub_buffer(buffer);
  }
  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override {
    return detail::upload_stub_buffer(buffer, data, byte_size);
  }

 private:
  HWND hwnd_ = nullptr;
};

}  // namespace

Device* create_gl_device() {
  return new GlDevice();
}

}  // namespace rhi
}  // namespace render
