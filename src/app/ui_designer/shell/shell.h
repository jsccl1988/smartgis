// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_SHELL_SHELL_H_
#define APP_UI_DESIGNER_SHELL_SHELL_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/ui_designer/canvas/canvas.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/document/markup_document.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace app {

// Root editor chrome: palette, canvas, props, tree, diagnostics, and document
// session (load/save/hot-reload, selection, drag-reorder, text2ui).
class DesignerShell : public ui::views::View {
 public:
  bool on_key_event(const ui::views::KeyEvent& e) override;

  void build_ui();
  void open_path(const std::string& path);
  void tick_hot_reload();
  void tick_generate_hotkey();
  void toggle_diagnostics();

 private:
  static ui::views::Textfield* add_prop(ui::views::View* host,
                                        const char* caption);
  static void add_btn(ui::views::View* host,
                      const char* text,
                      std::function<void()> fn);

  void status_set(const std::string& s);
  void open_file();
  void save_file(bool save_as);
  void reload(bool announce);
  void rebuild_from_dom();
  void rebuild_tree();
  std::string id_for_hit(ui::views::View* hit) const;
  ui::views::Rect view_bounds(ui::views::View* v) const;
  bool on_canvas_mouse(const ui::views::MouseEvent& e);
  void clear_drag();

  struct SiblingSlot {
    std::string id;
    ui::views::Rect bounds;
  };

  std::vector<SiblingSlot> sibling_slots(const std::string& moving_id) const;
  static bool siblings_use_horizontal(const std::vector<SiblingSlot>& slots);
  void update_drop_target(int widget_x, int widget_y);
  void apply_drop_reorder(const std::string& moving,
                          const std::string& before_id,
                          bool append);
  void persist_and_rebuild(const std::string& status);
  void highlight();
  void sync_props();
  void apply_properties();
  void insert_pending();
  void run_generate();
  void reorder_selected(int delta);

  std::string path_;
  std::string css_path_;
  uint64_t xml_mtime_ = 0;
  uint64_t css_mtime_ = 0;
  bool suppress_ = false;
  ui::views::MarkupDocument document_;
  ui::views::NamedViewMap ids_;
  std::string selected_id_;
  std::string pending_insert_;
  int id_seq_ = 0;
  int anon_seq_ = 0;
  bool drag_armed_ = false;
  bool dragging_ = false;
  int drag_start_x_ = 0;
  int drag_start_y_ = 0;
  std::string drag_id_;
  std::string drop_before_id_;
  bool drop_append_ = false;
  CanvasHost* canvas_ = nullptr;
  ui::views::TreeView* tree_ = nullptr;
  ui::views::Label* status_ = nullptr;
  ui::views::Textfield* prop_id_ = nullptr;
  ui::views::Textfield* prop_text_ = nullptr;
  ui::views::Textfield* prop_class_ = nullptr;
  ui::views::Textfield* prop_width_ = nullptr;
  ui::views::Textfield* prop_height_ = nullptr;
  ui::views::Textfield* prop_flex_ = nullptr;
  bool generate_hotkey_latched_ = false;
  ui::views::DiagnosticToolsPanel* diagnostics_ = nullptr;
};

}  // namespace app

#endif  // APP_UI_DESIGNER_SHELL_SHELL_H_
