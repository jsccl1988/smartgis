// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_PANELS_INSPECTOR_SYNC_CHROME_H_
#define APP_VIEWS_SHELL_UI_PANELS_INSPECTOR_SYNC_CHROME_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// FeatureInfo / AttributeTable / playback sync and edit feedback.
class InspectorSyncChrome {
 public:
  explicit InspectorSyncChrome(BrowserView* host);
  ~InspectorSyncChrome() = default;

  InspectorSyncChrome(const InspectorSyncChrome&) = delete;
  InspectorSyncChrome& operator=(const InspectorSyncChrome&) = delete;

  void sync_inspectors_from_scene();
  void sync_result_playback_from_session();
  void wire_edit_feedback();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_PANELS_INSPECTOR_SYNC_CHROME_H_
