// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_GPU_H
#define GPU_GPU_H

#include <cstdint>

// GPU / render process payload. Linked into the chrome PE so
// ContentMain can dispatch --type=gpu in the same image.
namespace gpu {

class SmtAdapter {
 public:
  virtual ~SmtAdapter() = default;
  virtual bool load_legacy_dlls() = 0;
  virtual bool bind_view(uint32_t view_id, void* legacy_map) = 0;
  virtual void* render_device(uint32_t view_id) = 0;
  virtual bool init_hidden_hwnd(int width_px, int height_px) = 0;
  virtual void* hwnd() const = 0;
};

SmtAdapter* create_smt_adapter();
int render_main(int argc, wchar_t** argv);
int run_self_test(const wchar_t* exe_path);

}  // namespace gpu

#endif  // GPU_GPU_H
