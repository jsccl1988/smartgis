// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/inspect/feature_info.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {
namespace {

constexpr int kPreferredWDip = 300;
constexpr int kPreferredHDip = 220;

bool is_section_header(const std::string& name) {
  return name.size() >= 3 && name[0] == '-' && name[1] == '-' && name[2] == '-';
}

std::string display_field_name(const std::string& name) {
  if (!is_section_header(name)) {
    return name;
  }
  // "--- Feature attributes ---" → "Feature attributes"
  size_t begin = 0;
  while (begin < name.size() && (name[begin] == '-' || name[begin] == ' ')) {
    ++begin;
  }
  size_t end = name.size();
  while (end > begin && (name[end - 1] == '-' || name[end - 1] == ' ')) {
    --end;
  }
  if (begin >= end) {
    return name;
  }
  return name.substr(begin, end - begin);
}

std::string display_field_value(const std::string& value) {
  if (value.empty()) {
    return "\xE2\x80\x94";  // em dash —
  }
  // Common null spellings from hosts / OGR.
  if (value == "null" || value == "NULL" || value == "Null" ||
      value == "<null>" || value == "(null)") {
    return "\xE2\x80\x94";
  }
  return value;
}

std::string title_case_geom(std::string geom) {
  if (geom.empty()) {
    return geom;
  }
  geom[0] = static_cast<char>(
      std::toupper(static_cast<unsigned char>(geom[0])));
  for (size_t i = 1; i < geom.size(); ++i) {
    geom[i] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(geom[i])));
  }
  return geom;
}

}  // namespace

FeatureInfo::FeatureInfo() {
  set_focusable(true);
  MarkupRoot loaded = load_markup("inspect/feature_info.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({kPreferredWDip, kPreferredHDip});
    return;
  }
  // Prefer new identity labels; fall back to legacy "id" markup id.
  layer_label_ = loaded.ids.find_as<Label>("layer");
  if (!layer_label_) {
    layer_label_ = loaded.ids.find_as<Label>("id");
  }
  meta_label_ = loaded.ids.find_as<Label>("meta");
  nav_label_ = loaded.ids.find_as<Label>("nav");
  prev_ = loaded.ids.find_as<Button>("prev");
  next_ = loaded.ids.find_as<Button>("next");
  table_ = loaded.ids.find_as<TableView>("table");

  if (prev_) {
    prev_->set_text("<");
    prev_->set_click([this]() { show_prev_hit(); });
  }
  if (next_) {
    next_->set_text(">");
    next_->set_click([this]() { show_next_hit(); });
  }
  if (table_) {
    table_->set_row_click([this](int row) { on_field_row_click(row); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({kPreferredWDip, kPreferredHDip});
  add_child(std::move(loaded.root));
  set_preferred_size({kPreferredWDip, kPreferredHDip});
  refresh_frame();
  rebuild_table();
}

FeatureInfo::~FeatureInfo() {
  if (prev_) {
    prev_->set_click({});
  }
  if (next_) {
    next_->set_click({});
  }
  if (table_) {
    table_->set_row_click({});
  }
  remove_all_children();
  layer_label_ = nullptr;
  meta_label_ = nullptr;
  nav_label_ = nullptr;
  prev_ = nullptr;
  next_ = nullptr;
  table_ = nullptr;
}

void FeatureInfo::set_feature_id(std::string id) {
  feature_id_ = std::move(id);
  refresh_frame();
}

void FeatureInfo::set_layer_name(std::string name) {
  layer_name_ = std::move(name);
  refresh_frame();
}

void FeatureInfo::set_geometry_type(std::string type) {
  geometry_type_ = title_case_geom(std::move(type));
  refresh_frame();
}

void FeatureInfo::set_fields(const std::vector<Field>& fields) {
  fields_ = fields;
  field_count_ = fields_.size();
  hits_.clear();
  active_hit_ = 0;
  absorb_identity_from_fields();
  refresh_frame();
  rebuild_table();
}

void FeatureInfo::clear() {
  feature_id_.clear();
  layer_name_.clear();
  geometry_type_.clear();
  fields_.clear();
  field_count_ = 0;
  hits_.clear();
  active_hit_ = 0;
  refresh_frame();
  rebuild_table();
}

void FeatureInfo::set_hits(std::vector<Hit> hits, size_t active_index) {
  hits_ = std::move(hits);
  if (hits_.empty()) {
    clear();
    return;
  }
  if (active_index >= hits_.size()) {
    active_index = 0;
  }
  apply_hit(active_index);
}

bool FeatureInfo::select_hit(size_t index) {
  if (hits_.empty() || index >= hits_.size()) {
    return false;
  }
  if (index == active_hit_) {
    return true;
  }
  apply_hit(index);
  if (hit_changed_) {
    hit_changed_(active_hit_);
  }
  return true;
}

bool FeatureInfo::show_prev_hit() {
  if (hits_.size() < 2 || active_hit_ == 0) {
    return false;
  }
  return select_hit(active_hit_ - 1);
}

bool FeatureInfo::show_next_hit() {
  if (hits_.size() < 2 || active_hit_ + 1 >= hits_.size()) {
    return false;
  }
  return select_hit(active_hit_ + 1);
}

int FeatureInfo::selected_field_row() const {
  return table_ ? table_->selected_row() : -1;
}

void FeatureInfo::set_hit_changed(HitChanged fn) {
  hit_changed_ = std::move(fn);
}

void FeatureInfo::set_field_selected(FieldSelected fn) {
  field_selected_ = std::move(fn);
}

bool FeatureInfo::on_key_event(const KeyEvent& event) {
  if (event.type == KeyEvent::Type::kDown) {
    if (event.vk == VK_LEFT) {
      return show_prev_hit();
    }
    if (event.vk == VK_RIGHT) {
      return show_next_hit();
    }
    if (table_ && table_->row_count() > 0 &&
        (event.vk == VK_UP || event.vk == VK_DOWN)) {
      const int cur = table_->selected_row();
      const int n = static_cast<int>(table_->row_count());
      int next = cur;
      if (event.vk == VK_UP) {
        next = cur <= 0 ? 0 : cur - 1;
      } else {
        next = cur < 0 ? 0 : std::min(cur + 1, n - 1);
      }
      if (next != cur) {
        table_->set_selected_row(next);
        on_field_row_click(next);
      }
      return true;
    }
  }
  return View::on_key_event(event);
}

void FeatureInfo::on_device_scale_factor_changed(float old_scale,
                                               float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(kPreferredWDip, s), dip_to_px(kPreferredHDip, s)});
}

void FeatureInfo::absorb_identity_from_fields() {
  for (const auto& field : fields_) {
    if (layer_name_.empty() &&
        (field.name == "source-layer" || field.name == "Layer" ||
         field.name == "layer")) {
      layer_name_ = field.value;
    }
    if (geometry_type_.empty() &&
        (field.name == "geom" || field.name == "Geometry" ||
         field.name == "geometry" || field.name == "Shape")) {
      geometry_type_ = title_case_geom(field.value);
    }
  }
}

void FeatureInfo::apply_hit(size_t index) {
  active_hit_ = index;
  const Hit& hit = hits_[index];
  feature_id_ = hit.feature_id;
  layer_name_ = hit.layer_name;
  geometry_type_ = title_case_geom(hit.geometry_type);
  fields_ = hit.fields;
  field_count_ = fields_.size();
  absorb_identity_from_fields();
  refresh_frame();
  rebuild_table();
}

void FeatureInfo::on_field_row_click(int row) {
  if (field_selected_) {
    field_selected_(row);
  }
}

void FeatureInfo::sync_nav_enabled() {
  const bool multi = hits_.size() > 1;
  if (prev_) {
    prev_->set_enabled(multi && active_hit_ > 0);
    prev_->set_visible(multi);
  }
  if (next_) {
    next_->set_enabled(multi && active_hit_ + 1 < hits_.size());
    next_->set_visible(multi);
  }
  if (nav_label_) {
    nav_label_->set_visible(multi);
  }
}

void FeatureInfo::refresh_frame() {
  const Theme& t = Theme::current();
  const bool empty =
      fields_.empty() && feature_id_.empty() && layer_name_.empty();

  if (layer_label_) {
    if (empty) {
      layer_label_->set_text("No feature identified");
      layer_label_->set_color(t.text_muted);
    } else if (!layer_name_.empty()) {
      layer_label_->set_text(layer_name_);
      layer_label_->set_color(t.text_bright);
    } else if (!feature_id_.empty()) {
      layer_label_->set_text(feature_id_);
      layer_label_->set_color(t.text_bright);
    } else {
      layer_label_->set_text("Feature");
      layer_label_->set_color(t.text_bright);
    }
  }

  if (meta_label_) {
    if (empty) {
      meta_label_->set_text("Click a map feature to inspect attributes");
      meta_label_->set_color(t.text_muted);
    } else {
      std::string meta;
      if (!geometry_type_.empty()) {
        meta = geometry_type_;
      }
      if (!feature_id_.empty()) {
        if (!meta.empty()) {
          meta += "  ·  ";
        }
        meta += "OID ";
        meta += feature_id_;
      }
      if (meta.empty()) {
        meta = "Attributes";
      }
      meta_label_->set_text(std::move(meta));
      meta_label_->set_color(t.text_muted);
    }
  }

  if (nav_label_) {
    if (hits_.size() > 1) {
      nav_label_->set_text(std::to_string(active_hit_ + 1) + " of " +
                           std::to_string(hits_.size()));
      nav_label_->set_color(t.text);
    } else {
      nav_label_->set_text("");
    }
  }

  sync_nav_enabled();
  schedule_paint();
}

void FeatureInfo::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  table_->set_columns({"Field", "Value"});
  if (fields_.empty()) {
    const bool idle =
        feature_id_.empty() && layer_name_.empty() && geometry_type_.empty();
    table_->add_row(
        {"", idle ? "Click a map feature to inspect" : "No attributes"});
  } else {
    for (const auto& field : fields_) {
      const std::string name = display_field_name(field.name);
      if (is_section_header(field.name)) {
        // Section banner: name only, empty value (em dash suppressed).
        table_->add_row({name, ""});
      } else {
        table_->add_row({name, display_field_value(field.value)});
      }
    }
  }
  const int width =
      bounds().width > 0 ? bounds().width : preferred_size().width;
  const int height = table_->header_height() +
                     static_cast<int>(table_->row_count()) * table_->row_height();
  table_->set_preferred_size({width, height});
}

void FeatureInfo::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  // Calm top hairline so the identify chrome separates from the host tab strip.
  float scale = 1.f;
  if (widget()) {
    scale = widget()->device_scale_factor();
  }
  const int hair = std::max(1, dip_to_px(1, scale));
  canvas->fill_rect(b.x, b.y, b.width, hair, t.panel_header);
}

}  // namespace views
}  // namespace ui
