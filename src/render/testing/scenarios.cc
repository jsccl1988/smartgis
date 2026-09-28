// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/testing/scenarios.h"

#include <memory>

#include "render/graph/frame_graph.h"
#include "render/programs/programs.h"

namespace render {
namespace detail {
namespace {

ScenarioResult fail(const char* msg) {
  ScenarioResult r;
  r.ok = false;
  r.message = msg;
  return r;
}

ScenarioResult ok_msg(const char* msg) {
  ScenarioResult r;
  r.ok = true;
  r.message = msg;
  return r;
}

ScenarioResult skip_msg(const char* msg) {
  ScenarioResult r;
  r.ok = true;
  r.skipped = true;
  r.message = msg;
  return r;
}

class CountingEffect : public graph::Effect {
 public:
  explicit CountingEffect(graph::EffectSlot slot) : slot_(slot) {}
  graph::EffectSlot slot() const override { return slot_; }
  bool clears_color() const override {
    return slot_ == graph::EffectSlot::kBeforeOpaque;
  }
  bool record(const graph::RecordContext&) override {
    ++calls;
    return true;
  }
  int calls = 0;

 private:
  graph::EffectSlot slot_;
};

}  // namespace

rhi::Device* create_initialized_device(rhi::Backend backend,
                                       const rhi::DeviceDesc& desc) {
  rhi::Device* device = rhi::create_device(backend);
  if (!device) {
    return nullptr;
  }
  if (!device->initialize(desc)) {
    delete device;
    return nullptr;
  }
  return device;
}

ScenarioResult run_device_lifecycle(rhi::Backend backend) {
  std::unique_ptr<rhi::Device> device(rhi::create_device(backend));
  if (!device) {
    return fail("create_device returned null");
  }
  if (!device->initialize(rhi::DeviceDesc())) {
    return fail("initialize failed");
  }
  if (device->backend() != backend) {
    return fail("backend mismatch");
  }
  device->shutdown();
  return ok_msg("device_lifecycle");
}

ScenarioResult run_resources(rhi::Device* device) {
  if (!device) {
    return fail("null device");
  }
  rhi::Buffer* vb = device->create_buffer(12, rhi::BufferUsage::kVertex);
  if (!vb) {
    return fail("create_buffer failed");
  }
  const float xyz[3] = {1.f, 2.f, 3.f};
  if (!device->upload(vb, xyz, 12)) {
    device->destroy_buffer(vb);
    return fail("upload buffer failed");
  }
  rhi::TextureDesc tex_desc;
  tex_desc.width = 2;
  tex_desc.height = 2;
  tex_desc.format = rhi::TextureFormat::kRgba8;
  rhi::Texture* tex = device->create_texture(tex_desc);
  if (!tex || tex->width() != 2 || tex->height() != 2) {
    device->destroy_buffer(vb);
    device->destroy_texture(tex);
    return fail("create_texture failed");
  }
  const unsigned char rgba[16] = {1, 2, 3, 4, 5, 6, 7, 8,
                                  9, 10, 11, 12, 13, 14, 15, 16};
  if (!device->upload_texture(tex, rgba, 16)) {
    device->destroy_buffer(vb);
    device->destroy_texture(tex);
    return fail("upload_texture failed");
  }
  device->destroy_texture(tex);
  device->destroy_buffer(vb);
  return ok_msg("resources");
}

ScenarioResult run_pipeline_bind(rhi::Device* device) {
  if (!device) {
    return fail("null device");
  }
  rhi::Pipeline* pipe =
      device->create_graphics_pipeline(programs::solid_pipeline_desc());
  if (!pipe) {
    return fail("create_graphics_pipeline failed");
  }
  rhi::CommandList* list = device->create_command_list();
  if (!list) {
    device->destroy_pipeline(pipe);
    return fail("create_command_list failed");
  }
  list->set_pipeline(pipe);
  const programs::Color color{0.2f, 0.4f, 0.6f, 1.f};
  list->set_constants(programs::kColorSlot, &color, sizeof(color));
  list->close();
  if (!device->execute(list)) {
    device->destroy_command_list(list);
    device->destroy_pipeline(pipe);
    return fail("execute failed");
  }
  // Keep one list (FlyCube headless create/destroy cycles can hang).
  device->destroy_pipeline(pipe);
  return ok_msg("pipeline_bind");
}

ScenarioResult run_record_present(rhi::Device* device) {
  if (!device) {
    return fail("null device");
  }
  rhi::CommandList* list = device->create_command_list();
  if (!list) {
    return fail("create_command_list failed");
  }
  rhi::RenderPassDesc pass;
  pass.width = 64;
  pass.height = 64;
  list->begin_render_pass(pass);
  list->set_viewport(0, 0, 64, 64, 0, 1);
  list->draw_indexed(3, 1, 0, 0, 0);
  list->end_render_pass();
  list->close();
  if (!device->execute(list)) {
    return fail("execute failed");
  }
  device->present();
  return ok_msg("record_present");
}

ScenarioResult run_graph_present(rhi::Device* device) {
  if (!device) {
    return fail("null device");
  }
  CountingEffect before(graph::EffectSlot::kBeforeOpaque);
  CountingEffect opaque(graph::EffectSlot::kOpaque);
  CountingEffect after(graph::EffectSlot::kAfterOpaque);
  CountingEffect overlay(graph::EffectSlot::kOverlay);
  graph::ViewInput in;
  in.width_px = 64;
  in.height_px = 64;
  in.effects.push_back(&overlay);
  in.effects.push_back(&after);
  in.effects.push_back(&opaque);
  in.effects.push_back(&before);
  if (!graph::present(device, in)) {
    return fail("graph::present failed");
  }
  if (before.calls != 1 || opaque.calls != 1 || after.calls != 1 ||
      overlay.calls != 1) {
    return fail("effect call counts");
  }
  return ok_msg("graph_present");
}

ScenarioResult run_compute_smoke(rhi::Device* device) {
  if (!device) {
    return fail("null device");
  }
  if (!device->supports_compute()) {
    return skip_msg("compute unsupported");
  }
  rhi::ComputePipelineDesc desc;
  rhi::Pipeline* pipe = device->create_compute_pipeline(desc);
  if (!pipe) {
    return fail("create_compute_pipeline failed");
  }
  rhi::CommandList* list = device->create_command_list();
  if (!list) {
    device->destroy_pipeline(pipe);
    return fail("create_command_list failed");
  }
  list->set_pipeline(pipe);
  list->dispatch(1, 1, 1);
  list->close();
  const bool executed = device->execute(list);
  device->destroy_pipeline(pipe);
  if (!executed) {
    return fail("compute execute failed");
  }
  return ok_msg("compute_smoke");
}

ScenarioResult run_null_suite() {
  ScenarioResult r = run_device_lifecycle(rhi::Backend::kNull);
  if (!r.ok) {
    return r;
  }
  std::unique_ptr<rhi::Device> device(
      create_initialized_device(rhi::Backend::kNull, rhi::DeviceDesc()));
  if (!device) {
    return fail("null device init");
  }
  using StepFn = ScenarioResult (*)(rhi::Device*);
  const StepFn steps[] = {
      run_resources,     run_pipeline_bind, run_record_present,
      run_graph_present, run_compute_smoke,
  };
  for (StepFn step : steps) {
    r = step(device.get());
    if (!r.ok) {
      return r;
    }
  }
  device->shutdown();
  return ok_msg("null_suite");
}

}  // namespace detail
}  // namespace render
