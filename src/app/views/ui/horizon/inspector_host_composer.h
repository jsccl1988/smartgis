// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_INSPECTOR_HOST_COMPOSER_H_
#define APP_VIEWS_UI_HORIZON_INSPECTOR_HOST_COMPOSER_H_

#include "app/views/ui/horizon/inspector_group_bar.h"

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
  void bind_group_bar(InspectorGroupBar* bar);
  // |materialize| false selects the group and tab but leaves the placeholder.
  // Startup uses that so the Plugins page is the default dock without scanning
  // plugins on the first-map path.
  void show_inspector_group(InspectorGroupBar::Group group,
                            bool materialize = true);
  void note_tab_activated(int index);
  void sync_included_mask();
  void set_group_bar_extent(int width_px, int height_px);

 private:
  InspectorGroupBar::Group group_for_tab(int index) const;
  void apply_group(InspectorGroupBar::Group group, int prefer, bool materialize);

  BrowserView* host_ = nullptr;
  InspectorGroupBar* group_bar_ = nullptr;
  InspectorGroupBar::Group group_ = InspectorGroupBar::Group::kMap;
  int last_tab_[static_cast<int>(InspectorGroupBar::Group::kCount)] = {
      -1, -1, -1, -1};
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_INSPECTOR_HOST_COMPOSER_H_
