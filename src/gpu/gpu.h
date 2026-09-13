// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_GPU_H
#define GPU_GPU_H

#include <cstdint>

// GPU process payload. Linked into the chrome PE so ContentMain can
// dispatch --type=gpu in the same image. Renderer must not call this
// for paint; GpuMain owns D3D/GL.
namespace gpu {

class Adapter {
 public:
  virtual ~Adapter() = default;
  virtual bool load_legacy_dlls() = 0;
  virtual bool bind_view(uint32_t view_id, void* legacy_map) = 0;
  virtual void* render_device(uint32_t view_id) = 0;
  virtual bool init_hidden_hwnd(int width_px, int height_px) = 0;
  virtual void* hwnd() const = 0;
};

Adapter* create_adapter();
int GpuMain(int argc, wchar_t** argv);
int render_main(int argc, wchar_t** argv);
int run_self_test(const wchar_t* exe_path);

}  // namespace gpu

#endif  // GPU_GPU_H
