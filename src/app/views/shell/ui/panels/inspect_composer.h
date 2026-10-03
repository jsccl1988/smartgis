// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_PANELS_INSPECT_COMPOSER_H_
#define APP_VIEWS_SHELL_UI_PANELS_INSPECT_COMPOSER_H_

#include <functional>
#include <string>

namespace tool {
struct Draft;
}  // namespace tool

namespace app {

class BrowserView;

// Measure / selection / legend / layer-properties chrome wire.
class InspectComposer {
 public:
  explicit InspectComposer(BrowserView* host);
  ~InspectComposer() = default;

  InspectComposer(const InspectComposer&) = delete;
  InspectComposer& operator=(const InspectComposer&) = delete;

  void wire_measure_panel();
  void wire_selection_panel();
  void wire_layer_properties_panel();
  void wire_legend_panel();
  bool try_consume_measure_draft(const tool::Draft& draft);
  void sync_selection_panel_from_scene();
  void sync_legend_panel_from_scene();
  void sync_layer_properties_from_scene();
  bool invert_selection();
  bool export_selection_geojson(std::string* out_path);

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_PANELS_INSPECT_COMPOSER_H_
