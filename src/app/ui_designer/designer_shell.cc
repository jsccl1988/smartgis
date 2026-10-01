// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/designer_shell.h"

#include <algorithm>
#include <format>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "app/ui_designer/cursor_agent_llm.h"
#include "app/ui_designer/designer_dom.h"
#include "app/ui_designer/designer_io.h"
#include "app/ui_designer/designer_markup_edit.h"
#include "base/trace/event/process_trace.h"
#include "ui/gis/debug/debug_console_panel.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/views/dialogs/select_one_dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/text2ui/text2ui.h"

namespace app {

namespace {

using ui::views::BoxLayout;
using ui::views::Button;
using ui::views::ControlFactory;
using ui::views::DiagnosticToolsPanel;
using ui::views::KeyEvent;
using ui::views::Label;
using ui::views::MarkupOptions;
using ui::views::MarkupRoot;
using ui::views::MenuBar;
using ui::views::MenuItem;
using ui::views::MouseEvent;
using ui::views::Rect;
using ui::views::ScrollView;
using ui::views::Size;
using ui::views::Textfield;
using ui::views::ThemeService;
using ui::views::TreeView;
using ui::views::View;
using ui::views::build_markup_tree;

std::string css_path_for_xml(const std::string& xml_path) {
  std::string css = xml_path;
  const size_t dot = css.rfind(".ui.xml");
  if (dot != std::string::npos) {
    css.replace(dot, 7, ".ui.css");
  } else {
    css += ".ui.css";
  }
  return css;
}

void ensure_markup_preferred_size(View* root) {
  if (!root) {
    return;
  }
  Size pref = root->get_preferred_size();
  if (pref.width <= 0) {
    pref.width = 320;
  }
  if (pref.height <= 0) {
    pref.height = 160;
  }
  root->set_preferred_size(pref);
}

}  // namespace

bool DesignerShell::on_key_event(const KeyEvent& e) {
  if (e.type == KeyEvent::Type::kDown && e.vk == 'G' &&
      (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
      (GetKeyState(VK_SHIFT) & 0x8000) != 0) {
    // Latch so the 50ms poll timer does not open a second dialog.
    generate_hotkey_latched_ = true;
    run_generate();
    return true;
  }
  return View::on_key_event(e);
}

void DesignerShell::build_ui() {
  auto outer = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  auto menu = std::make_unique<MenuBar>();
  menu->set_preferred_size({1200, 28});
  menu->add_item("Open", [this]() { open_file(); });
  menu->add_item("Save", [this]() { save_file(false); });
  menu->add_item("Save As", [this]() { save_file(true); });
  menu->add_item("Reload", [this]() { reload(true); });
  menu->add_item("Generate...", [this]() { run_generate(); });
  {
    std::vector<MenuItem> view_items;
    view_items.push_back(
        {"Theme: Dark", []() { ThemeService::get().set_theme("dark"); }});
    view_items.push_back(
        {"Theme: Light", []() { ThemeService::get().set_theme("light"); }});
    view_items.push_back(
        {"Toggle Console+Trace", [this]() { toggle_diagnostics(); }});
    menu->add_menu("View", std::move(view_items));
  }

  auto body = std::make_unique<View>();
  auto body_box =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
  body_box->set_between_child_spacing(4);
  body->set_layout_manager(std::move(body_box));

  auto palette = std::make_unique<View>();
  {
    auto pb = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    pb->set_between_child_spacing(2);
    palette->set_layout_manager(std::move(pb));
    auto lab = std::make_unique<Label>("Palette");
    lab->set_preferred_size({140, 22});
    palette->add_child(std::move(lab));
    ControlFactory factory = ControlFactory::make_default();
    int tag_rows = 0;
    for (const std::string& tag : factory.registered_tags()) {
      if (tag == "tableview" || tag == "treeview" || tag == "scrollview" ||
          tag == "tabstrip") {
        continue;
      }
      auto btn = std::make_unique<Button>(tag);
      // Keep Button's measured height (min ~28); only clamp width for the
      // palette column — height 24 clipped glyphs (paint inset y+6).
      const Size natural = btn->preferred_size();
      const int bh = natural.height > 0 ? natural.height : 28;
      btn->set_preferred_size({130, bh});
      const std::string t = tag;
      btn->set_click([this, t]() {
        pending_insert_ = t;
        status_set("insert: " + t);
      });
      palette->add_child(std::move(btn));
      ++tag_rows;
    }
    // Intrinsic height from children so ScrollView can scroll overflow.
    palette->set_preferred_size({140, 22 + tag_rows * 32});
  }
  auto palette_scroll = std::make_unique<ScrollView>();
  // Match main_app.ui.css shell (~620x480) plus a little chrome margin so
  // Yoga is not forced to stack Map/Data/3D under a squeezed map_edit.
  palette_scroll->set_preferred_size({148, 500});
  palette_scroll->add_child(std::move(palette));

  auto canvas = std::make_unique<CanvasHost>();
  canvas_ = canvas.get();
  canvas_->set_preferred_size({640, 500});
  canvas_->set_mouse_handler(
      [this](const MouseEvent& e) { return on_canvas_mouse(e); });
  canvas_->set_content(nullptr);

  auto props = std::make_unique<View>();
  {
    auto pb = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    pb->set_between_child_spacing(4);
    props->set_layout_manager(std::move(pb));
    props->set_preferred_size({220, 500});
    prop_id_ = add_prop(props.get(), "id");
    prop_text_ = add_prop(props.get(), "text");
    prop_class_ = add_prop(props.get(), "class");
    prop_width_ = add_prop(props.get(), "width");
    prop_height_ = add_prop(props.get(), "height");
    prop_flex_ = add_prop(props.get(), "flex-grow");
    add_btn(props.get(), "Apply", [this]() { apply_properties(); });
    add_btn(props.get(), "Insert child", [this]() { insert_pending(); });
    add_btn(props.get(), "Move up", [this]() { reorder_selected(-1); });
    add_btn(props.get(), "Move down", [this]() { reorder_selected(1); });
  }

  View* canvas_view = canvas.get();
  body->add_child(std::move(palette_scroll));
  body->add_child(std::move(canvas));
  body->add_child(std::move(props));
  if (auto* box = static_cast<BoxLayout*>(body->layout_manager())) {
    box->set_flex_for_view(canvas_view, 1);
  }
  View* body_raw = body.get();

  auto tree = std::make_unique<TreeView>();
  tree_ = tree.get();
  tree_->set_preferred_size({1200, 120});
  tree_->set_selection_changed([this](const TreeView::NodeId& id) {
    selected_id_ = id;
    sync_props();
    highlight();
  });

  auto status = std::make_unique<Label>("Ready");
  status_ = status.get();
  status_->set_preferred_size({1200, 22});

  auto diagnostics = ui::views::make_diagnostic_tools_panel();
  diagnostics_ = diagnostics.get();
  diagnostics_->set_preferred_size({1200, 180});
  diagnostics_->set_console_submit([this](const std::string& line) {
    if (!diagnostics_ || !diagnostics_->console_pane()) {
      return;
    }
    if (line == "help" || line == "?") {
      diagnostics_->console_pane()->append_line(
          "commands: help | profile on|off | status");
      return;
    }
    if (line == "profile on") {
      base::trace::set_tracing_enabled(true);
      diagnostics_->console_pane()->append_line("tracing enabled");
      return;
    }
    if (line == "profile off") {
      base::trace::set_tracing_enabled(false);
      diagnostics_->console_pane()->append_line("tracing disabled");
      return;
    }
    if (line == "status") {
      diagnostics_->console_pane()->append_line(std::format(
          "tracing={} events={}",
          base::trace::tracing_enabled() ? "on" : "off",
          base::trace::process_trace().size()));
      return;
    }
    diagnostics_->console_pane()->append_line("unknown: " + line);
  });
  // Hidden by default — Console+Trace steals ~180px and squeezes the canvas.
  // Toggle via View → Toggle Console+Trace.
  diagnostics_->set_visible_tools(false);
  diagnostics_->set_preferred_size({1200, 0});

  set_layout_manager(std::move(outer));
  add_child(std::move(menu));
  add_child(std::move(body));
  add_child(std::move(tree));
  add_child(std::move(diagnostics));
  add_child(std::move(status));
  if (auto* box = static_cast<BoxLayout*>(layout_manager())) {
    box->set_flex_for_view(body_raw, 1);
  }
}

void DesignerShell::open_path(const std::string& path) {
  path_ = path;
  reload(true);
}

void DesignerShell::tick_hot_reload() {
  if (path_.empty() || suppress_) {
    return;
  }
  if (file_mtime(path_) != xml_mtime_ || file_mtime(css_path_) != css_mtime_) {
    reload(false);
  }
}

void DesignerShell::tick_generate_hotkey() {
  const bool down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 &&
                    (GetAsyncKeyState('G') & 0x8000) != 0;
  if (down && !generate_hotkey_latched_) {
    generate_hotkey_latched_ = true;
    run_generate();
  } else if (!down) {
    generate_hotkey_latched_ = false;
  }
}

void DesignerShell::toggle_diagnostics() {
  if (!diagnostics_) {
    return;
  }
  const bool next = !diagnostics_->is_tools_visible();
  diagnostics_->set_visible_tools(next);
  diagnostics_->set_preferred_size({1200, next ? 180 : 0});
  mark_needs_layout();
  invalidate();
  status_set(next ? "Console+Trace shown" : "Console+Trace hidden");
}

Textfield* DesignerShell::add_prop(View* host, const char* caption) {
  auto lab = std::make_unique<Label>(caption);
  lab->set_preferred_size({200, 18});
  host->add_child(std::move(lab));
  auto tf = std::make_unique<Textfield>();
  tf->set_preferred_size({200, 26});
  Textfield* raw = tf.get();
  host->add_child(std::move(tf));
  return raw;
}

void DesignerShell::add_btn(View* host,
                            const char* text,
                            std::function<void()> fn) {
  auto btn = std::make_unique<Button>(text);
  btn->set_preferred_size({120, 28});
  btn->set_click(std::move(fn));
  host->add_child(std::move(btn));
}

void DesignerShell::status_set(const std::string& s) {
  if (status_) {
    status_->set_text(s);
    status_->invalidate();
  }
}

void DesignerShell::open_file() {
  HWND owner = widget() ? widget()->hwnd() : nullptr;
  auto r = ui::views::pick_open_file(
      owner, L"UI Markup (*.ui.xml)\0*.ui.xml\0All\0*.*\0");
  if (r.accepted) {
    open_path(r.path);
  }
}

void DesignerShell::save_file(bool save_as) {
  if (save_as || path_.empty()) {
    HWND owner = widget() ? widget()->hwnd() : nullptr;
    auto r = ui::views::pick_save_file(
        owner, L"UI Markup (*.ui.xml)\0*.ui.xml\0All\0*.*\0");
    if (!r.accepted) {
      return;
    }
    path_ = r.path;
    css_path_ = css_path_for_xml(path_);
  }
  if (!document_.save_files(path_, css_path_)) {
    status_set("save failed");
    return;
  }
  xml_mtime_ = file_mtime(path_);
  css_mtime_ = file_mtime(css_path_);
  status_set("saved " + path_);
}

void DesignerShell::reload(bool announce) {
  suppress_ = true;
  clear_drag();
  const std::string keep = selected_id_;
  if (path_.empty()) {
    status_set("load failed: empty path");
    suppress_ = false;
    return;
  }

  // Resolve basename (e.g. shell/main_app.ui.xml) to <exe>/../ui/... once so
  // DOM / CSS / hot-reload / save all use the same disk path.
  const std::string resolved = ui::views::resolve_markup_path(path_);
  if (resolved.empty()) {
    status_set("load failed: markup: not found: " + path_);
    suppress_ = false;
    return;
  }
  path_ = resolved;

  std::string err;
  const std::string base = [&]() {
    const size_t slash = path_.find_last_of("/\\");
    return slash == std::string::npos ? std::string()
                                      : path_.substr(0, slash);
  }();
  css_path_ = css_path_for_xml(path_);

  // Single pipeline: editable DOM first, then View tree (no second ifstream
  // against an unresolved relative path).
  const std::string xml = read_file_utf8(path_);
  if (xml.empty() || !document_.load_xml(xml, base, &err)) {
    status_set("load failed: " +
               (err.empty() ? std::string("empty or unreadable: ") + path_
                            : err));
    suppress_ = false;
    return;
  }

  MarkupRoot built;
  MarkupOptions opt;
  if (!build_markup_tree(document_, opt, &built) || !built.ok()) {
    status_set("build failed: " + built.error);
    suppress_ = false;
    return;
  }

  ids_ = std::move(built.ids);
  if (canvas_) {
    // Match product dialogs: give Yoga root a stable preferred size so the
    // canvas FillLayout / BoxLayout flex pass has a non-zero content size.
    ensure_markup_preferred_size(built.root.get());
    canvas_->set_content(std::move(built.root));
  }
  xml_mtime_ = file_mtime(path_);
  css_mtime_ = file_mtime(css_path_);
  rebuild_tree();
  selected_id_ =
      (!keep.empty() && ids_.contains(keep)) ? keep : std::string();
  sync_props();
  highlight();
  status_set((announce ? "loaded " : "hot-reload ") + path_);
  suppress_ = false;
  mark_needs_layout();
  invalidate();
}

void DesignerShell::rebuild_from_dom() {
  MarkupRoot out;
  MarkupOptions opt;
  if (!build_markup_tree(document_, opt, &out) || !out.ok()) {
    status_set("rebuild failed: " + out.error);
    return;
  }
  ids_ = std::move(out.ids);
  if (canvas_) {
    ensure_markup_preferred_size(out.root.get());
    canvas_->set_content(std::move(out.root));
  }
  rebuild_tree();
  sync_props();
  highlight();
  mark_needs_layout();
  invalidate();
}

void DesignerShell::rebuild_tree() {
  if (!tree_) {
    return;
  }
  tree_->clear();
  pugi::xml_node root = content_root(document_.raw_xml());
  if (!root) {
    return;
  }
  std::function<void(pugi::xml_node, const std::string&)> walk =
      [&](pugi::xml_node node, const std::string& parent) {
        std::string id = node.attribute("id").as_string("");
        if (id.empty()) {
          id = std::string(node.name()) + "_" + std::to_string(++anon_seq_);
          node.append_attribute("id") = id.c_str();
        }
        tree_->add_node(parent, id, std::string(node.name()) + " #" + id,
                        false);
        for (pugi::xml_node child : node.children()) {
          if (child.type() != pugi::node_element) {
            continue;
          }
          const std::string name = child.name();
          if (name == "item" || name == "style") {
            continue;
          }
          walk(child, id);
        }
      };
  walk(root, "");
}

std::string DesignerShell::id_for_hit(View* hit) const {
  if (!hit) {
    return {};
  }
  std::string deepest;
  int best = -1;
  for (const auto& [id, view] : ids_.entries()) {
    int depth = 0;
    bool match = false;
    for (View* v = hit; v; v = v->parent(), ++depth) {
      if (v == view) {
        match = true;
        break;
      }
    }
    if (match && depth > best) {
      best = depth;
      deepest = id;
    }
  }
  return deepest;
}

Rect DesignerShell::view_bounds(View* v) const {
  if (!v) {
    return {};
  }
  // Views paint in widget space (see Label::paint_self).
  return v->bounds();
}

bool DesignerShell::on_canvas_mouse(const MouseEvent& e) {
  if (!canvas_) {
    return false;
  }
  if (e.type == MouseEvent::Type::kDown && e.button == 1) {
    View* hit = canvas_->hit_content(e.x, e.y);
    selected_id_ = id_for_hit(hit);
    if (!pending_insert_.empty()) {
      clear_drag();
      insert_pending();
      return true;
    }
    sync_props();
    highlight();
    if (!selected_id_.empty()) {
      drag_armed_ = true;
      dragging_ = false;
      drag_start_x_ = e.x;
      drag_start_y_ = e.y;
      drag_id_ = selected_id_;
      status_set("selected " + selected_id_);
    } else {
      clear_drag();
    }
    return true;
  }
  if (drag_armed_ &&
      (e.type == MouseEvent::Type::kMove ||
       (e.type == MouseEvent::Type::kUp && e.button == 1))) {
    if (e.type == MouseEvent::Type::kMove) {
      const int dx = e.x - drag_start_x_;
      const int dy = e.y - drag_start_y_;
      if (!dragging_ &&
          (dx * dx + dy * dy) >=
              kCanvasDragThresholdPx * kCanvasDragThresholdPx) {
        dragging_ = true;
        status_set("dragging " + drag_id_);
      }
      if (dragging_) {
        update_drop_target(e.x, e.y);
      }
      return true;
    }
    const bool did_drag = dragging_;
    const std::string before = drop_before_id_;
    const bool append = drop_append_;
    const std::string moving = drag_id_;
    clear_drag();
    if (did_drag) {
      selected_id_ = moving;
      apply_drop_reorder(moving, before, append);
    }
    return true;
  }
  return false;
}

void DesignerShell::clear_drag() {
  drag_armed_ = false;
  dragging_ = false;
  drag_id_.clear();
  drop_before_id_.clear();
  drop_append_ = false;
  if (canvas_) {
    canvas_->set_drop_line({});
  }
}

std::vector<DesignerShell::SiblingSlot> DesignerShell::sibling_slots(
    const std::string& moving_id) const {
  std::vector<SiblingSlot> slots;
  pugi::xml_node node =
      find_by_id(content_root(document_.raw_xml()), moving_id);
  if (!node || !node.parent()) {
    return slots;
  }
  for (pugi::xml_node sib : node.parent().children()) {
    if (sib.type() != pugi::node_element) {
      continue;
    }
    const std::string name = sib.name();
    if (name == "item" || name == "style") {
      continue;
    }
    const std::string id = sib.attribute("id").as_string("");
    if (id.empty() || id == moving_id) {
      continue;
    }
    View* v = ids_.find(id);
    if (!v) {
      continue;
    }
    slots.push_back(SiblingSlot{id, view_bounds(v)});
  }
  return slots;
}

bool DesignerShell::siblings_use_horizontal(
    const std::vector<SiblingSlot>& slots) {
  if (slots.size() < 2) {
    return false;
  }
  int min_x = slots[0].bounds.x;
  int max_x = slots[0].bounds.right();
  int min_y = slots[0].bounds.y;
  int max_y = slots[0].bounds.bottom();
  for (const auto& s : slots) {
    min_x = (std::min)(min_x, s.bounds.x);
    max_x = (std::max)(max_x, s.bounds.right());
    min_y = (std::min)(min_y, s.bounds.y);
    max_y = (std::max)(max_y, s.bounds.bottom());
  }
  return (max_x - min_x) >= (max_y - min_y);
}

void DesignerShell::update_drop_target(int widget_x, int widget_y) {
  drop_before_id_.clear();
  drop_append_ = false;
  if (!canvas_) {
    return;
  }
  auto slots = sibling_slots(drag_id_);
  if (slots.empty()) {
    canvas_->set_drop_line({});
    return;
  }
  const bool horizontal = siblings_use_horizontal(slots);
  std::sort(slots.begin(), slots.end(),
            [horizontal](const SiblingSlot& a, const SiblingSlot& b) {
              return horizontal ? a.bounds.x < b.bounds.x
                               : a.bounds.y < b.bounds.y;
            });
  const int pointer = horizontal ? widget_x : widget_y;
  std::string before_id;
  Rect line;
  for (const auto& s : slots) {
    const int mid = horizontal ? (s.bounds.x + s.bounds.width / 2)
                               : (s.bounds.y + s.bounds.height / 2);
    if (pointer < mid) {
      before_id = s.id;
      if (horizontal) {
        line = Rect{s.bounds.x - 1, s.bounds.y, 2, s.bounds.height};
      } else {
        line = Rect{s.bounds.x, s.bounds.y - 1, s.bounds.width, 2};
      }
      break;
    }
  }
  if (before_id.empty()) {
    drop_append_ = true;
    const auto& last = slots.back();
    if (horizontal) {
      line =
          Rect{last.bounds.right() - 1, last.bounds.y, 2, last.bounds.height};
    } else {
      line =
          Rect{last.bounds.x, last.bounds.bottom() - 1, last.bounds.width, 2};
    }
  } else {
    drop_before_id_ = before_id;
  }
  canvas_->set_drop_line(line);
}

void DesignerShell::apply_drop_reorder(const std::string& moving,
                                       const std::string& before_id,
                                       bool append) {
  const DropReorderResult r =
      apply_drop_reorder_xml(document_, moving, before_id, append);
  switch (r) {
    case DropReorderResult::kMissing:
      return;
    case DropReorderResult::kNoop:
      status_set("reorder noop");
      return;
    case DropReorderResult::kNotSibling:
      status_set("drop target not sibling");
      return;
    case DropReorderResult::kOk:
      break;
  }
  selected_id_ = moving;
  persist_and_rebuild("reordered");
}

void DesignerShell::persist_and_rebuild(const std::string& status) {
  if (!path_.empty()) {
    document_.save_files(path_, css_path_);
    reload(true);
  } else {
    rebuild_from_dom();
  }
  status_set(status);
}

void DesignerShell::highlight() {
  if (!canvas_) {
    return;
  }
  View* v = ids_.find(selected_id_);
  if (!v) {
    canvas_->set_selected_bounds({});
  } else {
    canvas_->set_selected_bounds(view_bounds(v));
  }
}

void DesignerShell::sync_props() {
  pugi::xml_node node =
      find_by_id(content_root(document_.raw_xml()), selected_id_);
  if (!node) {
    return;
  }
  if (prop_id_) {
    prop_id_->set_text(node.attribute("id").as_string(""));
  }
  if (prop_text_) {
    prop_text_->set_text(node.attribute("text").as_string(""));
  }
  if (prop_class_) {
    prop_class_->set_text(node.attribute("class").as_string(""));
  }
}

void DesignerShell::apply_properties() {
  pugi::xml_node node =
      find_by_id(content_root(document_.raw_xml()), selected_id_);
  if (!node) {
    status_set("no selection");
    return;
  }
  auto set_a = [&](const char* name, Textfield* tf) {
    if (!tf) {
      return;
    }
    if (auto a = node.attribute(name)) {
      a.set_value(tf->text().c_str());
    } else {
      node.append_attribute(name) = tf->text().c_str();
    }
  };
  if (prop_id_ && !prop_id_->text().empty()) {
    set_a("id", prop_id_);
    selected_id_ = prop_id_->text();
  }
  set_a("text", prop_text_);
  set_a("class", prop_class_);
  std::vector<std::pair<std::string, std::string>> decls;
  if (prop_width_ && !prop_width_->text().empty()) {
    decls.emplace_back("width", prop_width_->text() + "px");
  }
  if (prop_height_ && !prop_height_->text().empty()) {
    decls.emplace_back("height", prop_height_->text() + "px");
  }
  if (prop_flex_ && !prop_flex_->text().empty()) {
    decls.emplace_back("flex-grow", prop_flex_->text());
  }
  if (!decls.empty()) {
    document_.stylesheet().upsert_id_declarations(selected_id_, decls);
  }
  persist_and_rebuild("applied");
}

void DesignerShell::insert_pending() {
  if (pending_insert_.empty()) {
    status_set("pick a palette tag");
    return;
  }
  pugi::xml_node parent =
      find_by_id(content_root(document_.raw_xml()), selected_id_);
  if (!parent) {
    parent = content_root(document_.raw_xml());
  }
  if (!parent) {
    return;
  }
  pugi::xml_node child = parent.append_child(pending_insert_.c_str());
  const std::string new_id =
      pending_insert_ + "_" + std::to_string(++id_seq_);
  child.append_attribute("id") = new_id.c_str();
  if (pending_insert_ == "label" || pending_insert_ == "button") {
    child.append_attribute("text") = pending_insert_.c_str();
  }
  pending_insert_.clear();
  selected_id_ = new_id;
  persist_and_rebuild("inserted " + new_id);
}

void DesignerShell::run_generate() {
  HWND owner = widget() ? widget()->hwnd() : nullptr;
  std::string prompt;
  if (!ui::views::InputTextDialog::run(
          owner, L"Text2UI",
          "Describe UI (template), or @llm <prompt> for Cursor Agent",
          &prompt)) {
    return;
  }
  if (prompt.empty()) {
    status_set("generate cancelled: empty prompt");
    return;
  }

  std::string mode = "Insert under selection";
  {
    const std::vector<std::string> modes = {
        "Insert under selection",
        "Replace document",
    };
    if (!ui::views::SelectOneDialog::run(owner, modes, &mode)) {
      status_set("generate cancelled");
      return;
    }
  }
  const bool replace = (mode.find("Replace") != std::string::npos);

  CursorAgentLlmBackend llm(owner);
  ui::views::Text2UiRequest req;
  req.prompt = prompt;
  req.llm = &llm;
  ui::views::Text2UiResult result;
  status_set(ui::views::strip_llm_prefix(prompt, nullptr) ? "llm: running…"
                                                          : "template…");
  if (!ui::views::generate_text2ui(req, &result) || !result.ok) {
    status_set("generate failed: " + result.error);
    return;
  }
  std::string err;
  if (!apply_markup_fragment(document_, selected_id_, result.xml_fragment,
                             replace, &err)) {
    status_set(err);
    return;
  }
  persist_and_rebuild(std::string(result.used_llm ? "llm" : "template") +
                      (replace ? ": replaced" : ": inserted"));
}

void DesignerShell::reorder_selected(int delta) {
  if (!reorder_xml_sibling(document_, selected_id_, delta)) {
    return;
  }
  persist_and_rebuild("reordered");
}

}  // namespace app
