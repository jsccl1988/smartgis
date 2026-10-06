// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_DEBUG_RENDER_TRACE_PANEL_H_
#define UI_GIS_DEBUG_RENDER_TRACE_PANEL_H_

#include "ui/ui_export.h"

#include <memory>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Checkbox;
class Label;

// Chrome Trace-style render analysis for Map2d + Scene3d present paths.
// Reads base::trace::process_trace(); does not embed Perfetto UI.
// Allocate only via make_render_trace_panel() so new/delete stay in ui_views.dll.
class UI_EXPORT RenderTracePanel : public View {
 public:
  RenderTracePanel();
  ~RenderTracePanel() override;

  // When embedded in DiagnosticToolsPanel, host owns horizon; reserved for layout.
  void set_embedded(bool embedded);
  bool is_embedded() const { return embedded_; }

  void refresh_from_process_trace(bool schedule = true);
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

  // Snapshot owned by the panel; free helpers in the .cc read it.
  struct State;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void on_record();
  void on_stop();
  void on_clear();
  void on_export();
  void on_refresh();
  void update_status();

  Label* title_ = nullptr;
  Label* status_ = nullptr;
  Label* rollup_ = nullptr;
  View* toolbar_ = nullptr;
  View* filters_ = nullptr;
  Button* record_ = nullptr;
  Button* stop_ = nullptr;
  Button* clear_ = nullptr;
  Button* export_ = nullptr;
  Button* refresh_ = nullptr;
  Checkbox* arm_ = nullptr;
  Checkbox* show_map2d_ = nullptr;
  Checkbox* show_scene3d_ = nullptr;
  Checkbox* show_startup_ = nullptr;
  Checkbox* show_gdi_ = nullptr;
  Checkbox* show_ui_ = nullptr;

  std::unique_ptr<State> state_;
  bool embedded_ = false;
};

// Factory: allocate in ui_views.dll (required for cross-module View ownership).
UI_EXPORT std::unique_ptr<RenderTracePanel> make_render_trace_panel();

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_DEBUG_RENDER_TRACE_PANEL_H_
