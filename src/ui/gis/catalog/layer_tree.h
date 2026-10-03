// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_CATALOG_LAYER_TREE_H_
#define UI_GIS_CATALOG_LAYER_TREE_H_

#include "ui/ui_export.h"
#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Catalog layer kind for TOC glyphs. Hosts may leave kUnknown for flat lists.
enum class LayerKind {
  kUnknown = 0,
  kGroup,
  kVector,
  kRaster,
};

// Vertical layer list: expand (groups) + visibility checkbox + type glyph +
// name. IDs are opaque strings; this control never stores SmtLayer* or other
// GIS pointers. Nested |children| are optional — empty children keep the
// historical flat-list layout.
class UI_EXPORT LayerTree : public View {
 public:
  // One opaque layer node for host-driven populate (CatalogCall / open-map).
  // Defaults preserve flat-list callers (unknown kind, no children, expanded).
  struct LayerDesc {
    std::string id;
    std::string name;
    bool visible = true;
    bool active = false;
    LayerKind kind = LayerKind::kUnknown;
    bool expanded = true;
    std::vector<LayerDesc> children;
  };

  using VisibleChanged = std::function<void(const std::string& id, bool visible)>;
  using SelectionChanged = std::function<void(const std::string& id)>;
  using ContextRequested =
      std::function<void(const std::string& id, Point screen)>;

  LayerTree();
  ~LayerTree() override;

  void clear();
  void add_layer(std::string id, std::string name, bool visible);

  // Replace the tree from |layers|. Selects the first entry with active=true
  // (else the first visible row). Host sync: does not fire visible_changed or
  // selection_changed (avoids CatalogCall + inspector rebuild mid-populate).
  void set_layers(const std::vector<LayerDesc>& layers);

  // Select |id| as the active layer (fires selection_changed when it changes).
  void select_layer(const std::string& id);

  // Update checkbox state without notifying visible_changed (host sync).
  void set_layer_visible(const std::string& id, bool visible);

  void set_visible_changed(VisibleChanged fn);
  void set_selection_changed(SelectionChanged fn);
  void set_context_requested(ContextRequested fn);

  const std::string& selected_id() const { return selected_id_; }
  size_t layer_count() const { return rows_.size(); }

  // Fills opaque id/name/visibility for |index|. Returns false if out of range.
  bool layer_at(size_t index,
                std::string* id,
                std::string* name,
                bool* visible) const;

  // Remove |id| from the list. Returns false if missing.
  bool remove_layer(const std::string& id);

  // Move |id| by |delta| among siblings (-1 up / +1 down). Returns false if
  // blocked.
  bool move_layer(const std::string& id, int delta);

  // Layer on/off for |id|, not View::is_visible() (layout / paint / hit-test).
  bool is_layer_visible(const std::string& id) const;

  void layout() override;
  bool on_mouse_event(const MouseEvent& event) override;
  bool on_key_event(const KeyEvent& event) override;
  void on_device_scale_factor_changed(float old_scale, float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  class LayerRow;

  friend class LayerRow;

  float scale_factor() const;
  int row_height() const;
  int check_size() const;
  int check_pad() const;
  int label_gap() const;
  int rail_width() const;
  int indent_step() const;
  int expand_slot() const;
  int icon_slot() const;

  // Device-px geometry for ink / hit regions within |row_bounds|.
  Rect expand_ink(const Rect& row_bounds, int depth) const;
  Rect expand_hit(const Rect& row_bounds, int depth) const;
  // Checkbox ink rect for |row_bounds| (device px). Hit-test uses a padded
  // sibling so the toggle stays easy to click without enlarging the glyph.
  Rect checkbox_ink(const Rect& row_bounds, int depth) const;
  Rect checkbox_hit(const Rect& row_bounds, int depth) const;
  Rect type_icon_ink(const Rect& row_bounds, int depth) const;

  void clear_rows();
  void rebuild_from_model();
  void append_visible(const LayerDesc& node, int depth);
  LayerRow* acquire_row(std::string id,
                        std::string name,
                        bool visible,
                        LayerKind kind,
                        int depth,
                        bool expandable,
                        bool expanded);

  LayerDesc* find_desc(std::vector<LayerDesc>* nodes, const std::string& id);
  bool remove_desc(std::vector<LayerDesc>* nodes, const std::string& id);
  bool set_desc_visible(std::vector<LayerDesc>* nodes,
                        const std::string& id,
                        bool visible);
  bool toggle_desc_expanded(const std::string& id);
  std::vector<LayerDesc>* sibling_list(std::vector<LayerDesc>* nodes,
                                       const std::string& id);

  LayerRow* row_at(const std::string& id) const;
  LayerRow* row_at_point(int x, int y) const;
  void select_id(const std::string& id);
  void notify_visible(const std::string& id, bool visible);
  bool handle_row_mouse(LayerRow* row, const MouseEvent& event);
  bool handle_row_key(LayerRow* row, const KeyEvent& event);
  bool focus_row_delta(LayerRow* from, int delta);
  Point to_screen(int x, int y) const;

  bool batch_layout_ = false;

  // Authoritative tree; |rows_| is the depth-first expanded projection.
  std::vector<LayerDesc> model_;
  std::vector<LayerRow*> rows_;
  std::string selected_id_;
  VisibleChanged visible_changed_;
  SelectionChanged selection_changed_;
  ContextRequested context_requested_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_CATALOG_LAYER_TREE_H_
