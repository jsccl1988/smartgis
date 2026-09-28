// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_STYLE_LAYER_PROPERTIES_PANEL_H_
#define UI_GIS_STYLE_LAYER_PROPERTIES_PANEL_H_

#include "ui/ui_export.h"

#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Label;
class SymbologyPanel;
class TabStrip;

// Tab shell for layer properties: Symbology + read-only Source page.
class UI_EXPORT LayerPropertiesPanel : public View {
 public:
  LayerPropertiesPanel();
  ~LayerPropertiesPanel() override;

  SymbologyPanel* symbology() { return symbology_; }
  const SymbologyPanel* symbology() const { return symbology_; }

  void set_source_text(std::string text);
  const std::string& source_text() const { return source_text_; }

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  Label* title_ = nullptr;
  TabStrip* tabs_ = nullptr;
  SymbologyPanel* symbology_ = nullptr;
  Label* source_label_ = nullptr;
  std::string source_text_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_STYLE_LAYER_PROPERTIES_PANEL_H_
