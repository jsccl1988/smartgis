// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_STYLE_SYMBOLOGY_PANEL_H_
#define UI_GIS_STYLE_SYMBOLOGY_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Combobox;
class Label;
class TableView;
class Textfield;

// Layer paint editor: field picker + paint key/value rows + Apply.
// Host maps paint kv into StyleDocument / ResolvedPaint; panel stays string-only.
class UI_EXPORT SymbologyPanel : public View {
 public:
  using PaintKv = std::vector<std::pair<std::string, std::string>>;
  using ApplyFn = std::function<void(const PaintKv& paint)>;

  SymbologyPanel();
  ~SymbologyPanel() override;

  void set_layer(std::string layer_token, std::string geom_type);
  const std::string& layer_token() const { return layer_token_; }
  const std::string& geom_type() const { return geom_type_; }

  void set_fields(std::vector<std::string> fields);
  const std::string& selected_field() const;

  void set_paint(PaintKv paint);
  const PaintKv& paint() const { return paint_; }

  void set_apply_handler(ApplyFn fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_paint_table();
  void refresh_layer_label();
  void on_apply();
  void on_value_commit();

  Label* title_ = nullptr;
  Label* layer_label_ = nullptr;
  Combobox* field_ = nullptr;
  TableView* paint_table_ = nullptr;
  Textfield* value_edit_ = nullptr;
  Button* apply_ = nullptr;

  std::string layer_token_;
  std::string geom_type_;
  PaintKv paint_;
  ApplyFn apply_handler_;
  int selected_paint_row_ = -1;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_STYLE_SYMBOLOGY_PANEL_H_
