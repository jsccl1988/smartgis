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
  uint32_t execute_count() const override { return execute_calls_; }

  CommandList* create_command_list() override { return new StubCommandList(); }

  // Intentionally leak stub objects. The same process links FlyCube; repeated
  // operator delete of stub CommandList/Buffer/Texture has hung headless CI
  // (scene_gpu_test TIMEOUT, leftover_record_test flaky finish). Production
  // backends (FlyCube/GDI) still free their resources.
  void destroy_command_list(CommandList* list) override { (void)list; }
  bool execute(CommandList* list) override {
    auto* stub = static_cast<StubCommandList*>(list);
    if (stub == nullptr || !stub->closed) {
      return false;
    }
    ++execute_calls_;
    return true;
  }
  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override {
    return detail::make_stub_buffer(byte_size, usage);
  }
  void destroy_buffer(Buffer* buffer) override { (void)buffer; }
  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override {
    return detail::upload_stub_buffer(buffer, data, byte_size);
  }
  void destroy_texture(Texture* texture) override { (void)texture; }

 private:
  uint32_t execute_calls_ = 0;
};

}  // namespace

Device* create_null_device() {
  return new NullDevice();
}

}  // namespace rhi
}  // namespace render
