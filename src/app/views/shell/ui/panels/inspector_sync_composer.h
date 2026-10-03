// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_PANELS_INSPECTOR_SYNC_COMPOSER_H_
#define APP_VIEWS_SHELL_UI_PANELS_INSPECTOR_SYNC_COMPOSER_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// FeatureInfo / AttributeTable / playback sync and edit feedback.
class InspectorSyncComposer {
 public:
  explicit InspectorSyncComposer(BrowserView* host);
  ~InspectorSyncComposer() = default;

  InspectorSyncComposer(const InspectorSyncComposer&) = delete;
  InspectorSyncComposer& operator=(const InspectorSyncComposer&) = delete;

  void sync_inspectors_from_scene();
  void sync_result_playback_from_session();
  void wire_edit_feedback();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_PANELS_INSPECTOR_SYNC_COMPOSER_H_
