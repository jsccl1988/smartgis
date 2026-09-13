// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

// Null backend records camera / texture binds for tests.

namespace render {
namespace rhi {
namespace {

class NullDevice : public Device {
 public:
  bool initialize(const DeviceDesc&) override { return true; }
  void shutdown() override {}
  void present() override {}
  Backend backend() const override { return Backend::kNull; }

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
};

}  // namespace

Device* create_null_device() {
  return new NullDevice();
}

}  // namespace rhi
}  // namespace render
