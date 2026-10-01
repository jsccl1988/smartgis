// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_GEOCHEM_ANALYZE_DIALOG_H_
#define PLUGIN_GEOCHEM_ANALYZE_DIALOG_H_

#include <string>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Textfield;
}

namespace plugin {

// Collects CSV/vector + element params and runs geochem analysis.
class AnalyzeDialog : public ui::views::View {
 public:
  explicit AnalyzeDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_stats();
  void on_pick_input();
  void on_pick_output();
  std::string build_analyze_json() const;
  std::string build_stats_json() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* input_path_ = nullptr;
  ui::views::Textfield* vector_path_ = nullptr;
  ui::views::Textfield* output_path_ = nullptr;
  ui::views::Textfield* element_ = nullptr;
  ui::views::Textfield* correlate_ = nullptr;
  ui::views::Textfield* threshold_ = nullptr;
  ui::views::Textfield* cells_ = nullptr;
  ui::views::Textfield* classes_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_GEOCHEM_ANALYZE_DIALOG_H_
