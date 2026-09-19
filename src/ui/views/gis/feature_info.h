// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_GIS_FEATURE_INFO_H_
#define UI_VIEWS_GIS_FEATURE_INFO_H_

#include <string>
#include <vector>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

class Label;
class TableView;

// Read-only identify inspector: feature token plus name/value field pairs.
// Hosts pass strings only; this widget never stores SmtFeature*.
class FeatureInfo : public View {
 public:
  struct Field {
    std::string name;
    std::string value;
  };

  FeatureInfo();

  void set_feature_id(std::string id);
  const std::string& feature_id() const { return feature_id_; }

  void set_fields(const std::vector<Field>& fields);
  void clear();

  size_t field_count() const { return field_count_; }

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void rebuild_table();

  Label* id_label_ = nullptr;
  TableView* table_ = nullptr;
  std::string feature_id_;
  std::vector<Field> fields_;
  size_t field_count_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_GIS_FEATURE_INFO_H_
