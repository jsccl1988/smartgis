// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_PANELS_PROCESSING_CHROME_H_
#define APP_VIEWS_SHELL_UI_PANELS_PROCESSING_CHROME_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// Wires processing / playback / report / spatial / Python GIS chrome.
class ProcessingChrome {
 public:
  explicit ProcessingChrome(BrowserView* host);
  ~ProcessingChrome() = default;

  ProcessingChrome(const ProcessingChrome&) = delete;
  ProcessingChrome& operator=(const ProcessingChrome&) = delete;

  void wire_processing_panel();
  void wire_result_playback_panel();
  void wire_report_panel();
  void attach_report_plugin_bridge();
  void sync_result_playback_timer();
  void wire_spatial_analysis_panel();
  void run_processing_operator(const std::string& processing_id);
  void run_processing_operator(const std::string& processing_id, const std::string& distance);
  void bind_gis_python_bridge();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_PANELS_PROCESSING_CHROME_H_
