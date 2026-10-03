// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_INSPECT_FEATURE_INFO_H_
#define UI_GIS_INSPECT_FEATURE_INFO_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Label;
class TableView;

// Read-only identify inspector (ArcGIS / QGIS Identify Results style).
// Hosts pass opaque string identity + name/value pairs; never SmtFeature*.
// Supports multi-hit navigation when the host supplies a Hit list.
class UI_EXPORT FeatureInfo : public View {
 public:
  struct Field {
    std::string name;
    std::string value;
  };

  // One identify candidate. Hosts may push several hits from a pick.
  struct Hit {
    std::string feature_id;
    std::string layer_name;
    std::string geometry_type;
    std::vector<Field> fields;
  };

  using HitChanged = std::function<void(size_t index)>;
  using FieldSelected = std::function<void(int row)>;

  FeatureInfo();
  ~FeatureInfo() override;

  void set_feature_id(std::string id);
  const std::string& feature_id() const { return feature_id_; }

  void set_layer_name(std::string name);
  const std::string& layer_name() const { return layer_name_; }

  void set_geometry_type(std::string type);
  const std::string& geometry_type() const { return geometry_type_; }

  void set_fields(const std::vector<Field>& fields);
  void clear();

  // Replace the hit list and show |active_index|. Empty clears the panel.
  void set_hits(std::vector<Hit> hits, size_t active_index = 0);
  bool select_hit(size_t index);
  bool show_prev_hit();
  bool show_next_hit();
  size_t hit_count() const { return hits_.size(); }
  size_t active_hit_index() const { return active_hit_; }

  size_t field_count() const { return field_count_; }
  int selected_field_row() const;

  void set_hit_changed(HitChanged fn);
  void set_field_selected(FieldSelected fn);

  bool on_key_event(const KeyEvent& event) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_table();
  void refresh_frame();
  void absorb_identity_from_fields();
  void apply_hit(size_t index);
  void on_field_row_click(int row);
  void sync_nav_enabled();

  Label* layer_label_ = nullptr;
  Label* meta_label_ = nullptr;
  Label* nav_label_ = nullptr;
  Button* prev_ = nullptr;
  Button* next_ = nullptr;
  TableView* table_ = nullptr;

  std::string feature_id_;
  std::string layer_name_;
  std::string geometry_type_;
  std::vector<Field> fields_;
  size_t field_count_ = 0;

  std::vector<Hit> hits_;
  size_t active_hit_ = 0;

  HitChanged hit_changed_;
  FieldSelected field_selected_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_INSPECT_FEATURE_INFO_H_
