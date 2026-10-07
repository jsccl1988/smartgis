// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_INSPECTOR_HOST_COMPOSER_H_
#define APP_VIEWS_UI_HORIZON_INSPECTOR_HOST_COMPOSER_H_

namespace app {

class BrowserView;

// Right-dock inspector TabStrip: lazy page create, activate, plugin docks.
class InspectorHostComposer {
 public:
  explicit InspectorHostComposer(BrowserView* host);
  ~InspectorHostComposer() = default;

  InspectorHostComposer(const InspectorHostComposer&) = delete;
  InspectorHostComposer& operator=(const InspectorHostComposer&) = delete;

  void ensure_inspector_tab(int index);
  void show_inspector_tab_index(int index);
  void activate_inspector_tab(int index);
  void ensure_processing_panel();
  void on_processing();
  void show_feature_info_tab();
  void attach_plugin_shell_ui();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_INSPECTOR_HOST_COMPOSER_H_
