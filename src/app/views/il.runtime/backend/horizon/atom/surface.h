// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_SURFACE_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_SURFACE_H_

#include <string>
#include <vector>

#include "content/browser/capability/horizon.h"

namespace app {

class Browser;

namespace detail {

// Win32 virtual-key aliases used by Interact / Host inject.
unsigned vk_from_name(const std::string& name);

// Shell HWND liveness. No pass/fail.
bool fill_hwnd_status(Browser& browser, content::HwndStatus* out);

// Posts input and resizes one live shell HWND. Inject and window share the
// same alive-window check so slot binders do not re-resolve the handle.
class ShellSurface {
 public:
  explicit ShellSurface(Browser& browser);

  bool alive() const;
  void* hwnd_ptr() const;

  bool click(int x, int y, int button, int clicks) const;
  bool drag(int x0, int y0, int x1, int y1) const;
  bool wheel(int x, int y, int delta) const;
  bool path(const std::vector<int>& xs, const std::vector<int>& ys) const;
  bool key(unsigned vk, bool down) const;
  bool tap_key(unsigned vk) const;

  // "activate" / "resize". Any other name succeeds when the window is alive.
  bool action(const std::string& name, int w, int h) const;

 private:
  bool activate() const;
  bool resize(int w, int h) const;

  Browser* browser_ = nullptr;
};

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_SURFACE_H_
