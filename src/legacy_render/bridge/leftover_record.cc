// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy_render/bridge/leftover_record.h"

#include "base/style/style.h"
#include "base/style/stylemanager.h"
#include "sdb/map/map.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace render {
namespace scene {

LeftoverRecorder::LeftoverRecorder()
    : device_(nullptr),
      list_(nullptr),
      native_window_(nullptr),
      width_(0),
      height_(0),
      owned_device_(false),
      open_(false),
      pass_open_(false) {}

LeftoverRecorder::~LeftoverRecorder() { release(); }

void LeftoverRecorder::set_native_window(void* native_window) {
  native_window_ = native_window;
}

bool LeftoverRecorder::ensure_device() {
  if (device_) {
    return true;
  }
  render::rhi::DeviceDesc desc;
  desc.native_window = native_window_;
  desc.width = width_;
  desc.height = height_;

  // Prefer FlyCube only when a present HWND is available; headless tests and
  // map-less record keep the null backend.
  if (native_window_) {
    device_ = render::rhi::create_device(render::rhi::preferred_gpu_backend());
    owned_device_ = device_ != nullptr;
    if (device_ && device_->initialize(desc)) {
      return true;
    }
    if (owned_device_ && device_) {
      device_->shutdown();
      delete device_;
      device_ = nullptr;
      owned_device_ = false;
    }
  }

  device_ = render::rhi::create_device(render::rhi::Backend::kNull);
  owned_device_ = device_ != nullptr;
  return device_ && device_->initialize(render::rhi::DeviceDesc());
}

void LeftoverRecorder::destroy_list() {
  if (device_ && list_) {
    device_->destroy_command_list(list_);
  }
  list_ = nullptr;
  open_ = false;
  pass_open_ = false;
}

void LeftoverRecorder::clear_leftover_3d() {
  for (LeftoverGpuMesh& mesh : leftover_3d_) {
    destroy_leftover_mesh(device_, &mesh);
  }
  leftover_3d_.clear();
}

bool LeftoverRecorder::attach(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  release();
  device_ = device;
  owned_device_ = false;
  return true;
}

bool LeftoverRecorder::begin(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return false;
  }
  width_ = width;
  height_ = height;
  if (!ensure_device()) {
    return false;
  }
  if (pass_open_ && list_) {
    list_->end_render_pass();
    pass_open_ = false;
  }
  destroy_list();
  clear_leftover_3d();
  list_ = device_->create_command_list();
  if (!list_) {
    return false;
  }
  open_ = true;
  return true;
}

bool LeftoverRecorder::record_world(const sdb::scene::World& world) {
  if (!open_ || !list_ || !device_) {
    return false;
  }
  if (pass_open_) {
    list_->end_render_pass();
    pass_open_ = false;
  }
  gpu_.sync_from(world);
  return gpu_.record_draws(device_, list_, width_, height_);
}

bool LeftoverRecorder::record_map(const sdb::SmtMap* map) {
  if (!map) {
    return false;
  }
  base::Envelope env;
  map->get_envelope(env);
  if (env.is_init()) {
    gpu_.set_view_ortho(env.MinX, env.MinY, env.MaxX, env.MaxY);
  } else {
    gpu_.clear_view_ortho();
  }
  // Default leftover brush cyan; prefer first MapLayer style brush when the
  // style manager has resolved that name.
  // TODO: per-layer solid on GpuInstance / Node without pulling StyleManager
  // into sdb/scene; per-feature Feature::style() during tessellate.
  long brush = 0x00FFFF00;
  const int layer_count = map->GetLayerCount();
  for (int i = 0; i < layer_count; ++i) {
    const sdb::MapLayer* layer = map->GetMapLayer(i);
    if (!layer || layer->style_name().empty()) {
      continue;
    }
    base::SmtStyleManager* mgr = base::SmtStyleManager::get_singleton_ptr();
    if (!mgr) {
      break;
    }
    if (base::SmtStyle* style = mgr->get_style(layer->style_name().c_str())) {
      brush = style->get_brush_desc().lBrushColor;
      break;
    }
  }
  gpu_.set_solid_color_from_colorref(brush);
  sdb::scene::World world;
  world.attach_map(map);
  return record_world(world);
}

bool LeftoverRecorder::record_3d(render::SmtVertexBuffer* vb,
                                 render::SmtIndexBuffer* ib) {
  if (!open_ || !list_ || !device_) {
    return false;
  }
  LeftoverGpuMesh mesh;
  if (!upload_leftover_buffers(device_, vb, ib, &mesh)) {
    return false;
  }
  leftover_3d_.push_back(mesh);
  if (!pass_open_) {
    render::rhi::RenderPassDesc pass;
    pass.clear_r = 0;
    pass.clear_g = 0.2f;
    pass.clear_b = 0.4f;
    pass.clear_a = 1;
    pass.width = width_;
    pass.height = height_;
    list_->begin_render_pass(pass);
    list_->set_viewport(0, 0, static_cast<float>(width_),
                        static_cast<float>(height_), 0, 1);
    pass_open_ = true;
  }
  const float aspect = height_ > 0
                           ? static_cast<float>(width_) / static_cast<float>(height_)
                           : 1.f;
  list_->bind_camera(
      render::rhi::make_perspective_camera(0.785398f, aspect, 0.1f, 100.f));
  return record_leftover_draw(list_, leftover_3d_.back());
}

bool LeftoverRecorder::finish() {
  if (!list_) {
    return false;
  }
  if (pass_open_) {
    list_->end_render_pass();
    pass_open_ = false;
  }
  list_->close();
  const bool ok = device_ && device_->execute(list_);
  open_ = false;
  return ok;
}

void LeftoverRecorder::release() {
  if (pass_open_ && list_) {
    list_->end_render_pass();
    pass_open_ = false;
  }
  gpu_.release();
  clear_leftover_3d();
  destroy_list();
  if (owned_device_ && device_) {
    device_->shutdown();
    delete device_;
  }
  device_ = nullptr;
  owned_device_ = false;
  width_ = 0;
  height_ = 0;
}

namespace {

LeftoverRecorder* resolve_smt_render_session() {
#ifdef _WIN32
  using SessionFn = LeftoverRecorder* (*)();
  // Bridge leftover DLL (dll_stem = legacy_render), not endgame render.
  static const wchar_t* kNames[] = {L"legacy_render_d.dll",
                                    L"legacy_render.dll"};
  HMODULE module = nullptr;
  for (const wchar_t* name : kNames) {
    module = GetModuleHandleW(name);
    if (module) {
      break;
    }
  }
  if (!module) {
    for (const wchar_t* name : kNames) {
      module = LoadLibraryW(name);
      if (module) {
        break;
      }
    }
  }
  if (!module) {
    return nullptr;
  }
  auto* fn = reinterpret_cast<SessionFn>(
      GetProcAddress(module, "smt_leftover_session"));
  if (!fn) {
    return nullptr;
  }
  return fn();
#else
  return nullptr;
#endif
}

}  // namespace

LeftoverRecorder& leftover_session() {
  if (LeftoverRecorder* shared = resolve_smt_render_session()) {
    return *shared;
  }
  static LeftoverRecorder local;
  return local;
}

bool leftover_session_is_process_wide() {
  return resolve_smt_render_session() != nullptr;
}

}  // namespace scene
}  // namespace render
