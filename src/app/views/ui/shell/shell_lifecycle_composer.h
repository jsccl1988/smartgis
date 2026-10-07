// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_SHELL_SHELL_LIFECYCLE_COMPOSER_H_
#define APP_VIEWS_UI_SHELL_SHELL_LIFECYCLE_COMPOSER_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {

class BrowserView;

// Widget init / show / close / wheel-forward horizon. Owns the startup path
// that used to live inline on BrowserView (seed, bind presenters, first map).
class ShellLifecycleComposer {
 public:
  explicit ShellLifecycleComposer(BrowserView* host);
  ~ShellLifecycleComposer() = default;

  ShellLifecycleComposer(const ShellLifecycleComposer&) = delete;
  ShellLifecycleComposer& operator=(const ShellLifecycleComposer&) = delete;

  bool init_shell();
  void show_shell();
  void finish_deferred_shell_wiring();
  void prepare_shell_close();
  void install_shell_wheel_forward();
  void remove_shell_wheel_forward();

 private:
  static LRESULT CALLBACK shell_wheel_subclass_proc(HWND hwnd, UINT msg,
                                                    WPARAM wparam,
                                                    LPARAM lparam,
                                                    UINT_PTR id,
                                                    DWORD_PTR data);

  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_SHELL_SHELL_LIFECYCLE_COMPOSER_H_
