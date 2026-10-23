// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_SHELL_SHELL_LAYOUT_COMPOSER_H_
#define APP_VIEWS_UI_SHELL_SHELL_LAYOUT_COMPOSER_H_

#include <memory>

namespace ui {
namespace views {
class TabStrip;
class View;
}  // namespace views
}  // namespace ui

namespace app {

class BrowserView;

// Loads shell/main_app.ui.xml and mounts real Catalog / Map / Ambox /
// inspector / Diagnostic / Status widgets into markup hosts. Falls back to
// the former imperative tree when markup fails to load.
class ShellLayoutComposer {
 public:
  explicit ShellLayoutComposer(BrowserView* host);
  ~ShellLayoutComposer() = default;

  ShellLayoutComposer(const ShellLayoutComposer&) = delete;
  ShellLayoutComposer& operator=(const ShellLayoutComposer&) = delete;

  void build_contents();

 private:
  bool build_from_markup();
  void build_imperative();
  std::unique_ptr<ui::views::View> wrap_inspector(
      std::unique_ptr<ui::views::TabStrip> side);
  // Friend-only BrowserView catalog_* wiring (not anonymous-namespace helpers).
  std::unique_ptr<ui::views::TabStrip> make_catalog_mode_tabs();

  // Cross-DLL TabStrip::set_change_fn batons (BrowserView*); friend access.
  static void map_tab_change_trampoline(void* baton, int index);
  static void inspector_tab_change_trampoline(void* baton, int index);

  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_SHELL_SHELL_LAYOUT_COMPOSER_H_
