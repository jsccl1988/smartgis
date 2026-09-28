// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/designer_app.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pugixml.hpp"

#include <sstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/kernel/frame/frame_view.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/markup/style/css_parser.h"
#include "ui/views/markup/document/markup_document.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace app {
namespace {

using ui::views::BoxLayout;
using ui::views::Button;
using ui::views::ControlFactory;
using ui::views::FillLayout;
using ui::views::FrameView;
using ui::views::Label;
using ui::views::MarkupDocument;
using ui::views::MarkupOptions;
using ui::views::MarkupRoot;
using ui::views::MenuBar;
using ui::views::MenuItem;
using ui::views::MouseEvent;
using ui::views::NamedViewMap;
using ui::views::Rect;
using ui::views::Size;
using ui::views::Textfield;
using ui::views::Theme;
using ui::views::ThemeService;
using ui::views::TreeView;
using ui::views::View;
using ui::views::Widget;
using ui::views::build_markup_tree;

constexpr int kCanvasDragThresholdPx = 4;

uint64_t file_mtime(const std::string& path) {
  struct _stat64 st = {};
  if (_stat64(path.c_str(), &st) != 0) {
    // UTF-8 path may need wide stat on Windows.
    const int n =
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (n > 0) {
      std::wstring w(static_cast<size_t>(n - 1), L'\0');
      MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
      if (_wstat64(w.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_mtime);
      }
    }
    return 0;
  }
  return static_cast<uint64_t>(st.st_mtime);
}

std::string read_file_utf8(const std::string& path) {
  const int n =
      MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  if (n <= 0) {
    return {};
  }
  std::wstring w(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
  FILE* f = nullptr;
  if (_wfopen_s(&f, w.c_str(), L"rb") != 0 || !f) {
    return {};
  }
  std::ostringstream ss;
  char buf[4096];
  while (const size_t got = fread(buf, 1, sizeof(buf), f)) {
    ss.write(buf, static_cast<std::streamsize>(got));
  }
  fclose(f);
  return ss.str();
}

pugi::xml_node find_by_id(pugi::xml_node root, const std::string& id) {
  if (!root || id.empty()) {
    return {};
  }
  if (root.attribute("id").as_string("") == id) {
    return root;
  }
  for (pugi::xml_node child : root.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    pugi::xml_node hit = find_by_id(child, id);
    if (hit) {
      return hit;
    }
  }
  return {};
}

pugi::xml_node content_root(const pugi::xml_document* doc) {
  if (!doc) {
    return {};
  }
  pugi::xml_node ui = doc->child("ui");
  if (!ui) {
    ui = doc->document_element();
  }
  if (!ui) {
    return {};
  }
  for (pugi::xml_node child : ui.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (std::string_view(child.name()) == "style") {
      continue;
    }
    return child;
  }
  return {};
}

// Transparent top child: last in z-order so get_view_at hits it (get_view_at
// is not virtual). Captures press so Widget routes move/up here for drag.
class CanvasHitLayer : public View {
 public:
  using MouseFn = std::function<bool(const MouseEvent&)>;
  void set_mouse_handler(MouseFn fn) { mouse_ = std::move(fn); }
  void set_selected_bounds(Rect r) {
    selected_bounds_ = r;
    invalidate();
  }
  void set_drop_line(Rect r) {
    drop_line_ = r;
    invalidate();
  }
  bool on_mouse_event(const MouseEvent& e) override {
    return mouse_ ? mouse_(e) : false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Theme& t = Theme::current();
    // Local paint space is 0..size; overlays are stored canvas-local.
    if (selected_bounds_.width > 0) {
      canvas->stroke_rect(selected_bounds_.x, selected_bounds_.y,
                          selected_bounds_.width, selected_bounds_.height,
                          t.accent, 2);
    }
    if (drop_line_.width > 0 || drop_line_.height > 0) {
      canvas->fill_rect(drop_line_.x, drop_line_.y, drop_line_.width,
                        drop_line_.height, t.accent);
    }
  }

 private:
  MouseFn mouse_;
  Rect selected_bounds_{};
  Rect drop_line_{};
};

class CanvasHost : public View {
 public:
  using MouseFn = std::function<bool(const MouseEvent&)>;
  void set_mouse_handler(MouseFn fn) {
    mouse_ = std::move(fn);
    if (hit_) {
      hit_->set_mouse_handler(mouse_);
    }
  }
  void set_content(std::unique_ptr<View> content) {
    remove_all_children();
    content_ = nullptr;
    hit_ = nullptr;
    set_layout_manager(std::make_unique<FillLayout>());
    if (content) {
      content_ = content.get();
      add_child(std::move(content));
    }
    auto hit = std::make_unique<CanvasHitLayer>();
    hit_ = hit.get();
    if (mouse_) {
      hit_->set_mouse_handler(mouse_);
    }
    hit_->set_selected_bounds(selected_bounds_);
    hit_->set_drop_line(drop_line_);
    add_child(std::move(hit));
  }
  View* hit_content(int x, int y) const {
    return content_ ? content_->get_view_at(x, y) : nullptr;
  }
  void set_selected_bounds(Rect r) {
    selected_bounds_ = r;
    if (hit_) {
      hit_->set_selected_bounds(r);
    }
    invalidate();
  }
  void set_drop_line(Rect r) {
    drop_line_ = r;
    if (hit_) {
      hit_->set_drop_line(r);
    }
    invalidate();
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Theme& t = Theme::current();
    canvas->fill_rect(bounds().x, bounds().y, bounds().width, bounds().height,
                      t.shell_bg);
  }

 private:
  View* content_ = nullptr;
  CanvasHitLayer* hit_ = nullptr;
  MouseFn mouse_;
  Rect selected_bounds_{};
  Rect drop_line_{};
};

class DesignerShell : public View {
 public:
  void build_ui() {
    auto outer =
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
    auto menu = std::make_unique<MenuBar>();
    menu->set_preferred_size({1200, 28});
    menu->add_item("Open", [this]() { open_file(); });
    menu->add_item("Save", [this]() { save_file(false); });
    menu->add_item("Save As", [this]() { save_file(true); });
    menu->add_item("Reload", [this]() { reload(true); });
    {
      std::vector<MenuItem> view_items;
      view_items.push_back(
          {"Theme: Dark", []() { ThemeService::get().set_theme("dark"); }});
      view_items.push_back(
          {"Theme: Light", []() { ThemeService::get().set_theme("light"); }});
      menu->add_menu("View", std::move(view_items));
    }

    auto body = std::make_unique<View>();
    auto body_box =
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    body_box->set_between_child_spacing(4);
    body->set_layout_manager(std::move(body_box));

    auto palette = std::make_unique<View>();
    {
      auto pb =
          std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
      pb->set_between_child_spacing(2);
      palette->set_layout_manager(std::move(pb));
      palette->set_preferred_size({140, 600});
      auto lab = std::make_unique<Label>("Palette");
      lab->set_preferred_size({140, 22});
      palette->add_child(std::move(lab));
      ControlFactory factory = ControlFactory::make_default();
      for (const std::string& tag : factory.registered_tags()) {
        if (tag == "tableview" || tag == "treeview" || tag == "scrollview" ||
            tag == "tabstrip") {
          continue;
        }
        auto btn = std::make_unique<Button>(tag);
        btn->set_preferred_size({130, 24});
        const std::string t = tag;
        btn->set_click([this, t]() {
          pending_insert_ = t;
          status_set("insert: " + t);
        });
        palette->add_child(std::move(btn));
      }
    }

    auto canvas = std::make_unique<CanvasHost>();
    canvas_ = canvas.get();
    canvas_->set_preferred_size({640, 600});
    canvas_->set_mouse_handler(
        [this](const MouseEvent& e) { return on_canvas_mouse(e); });
    canvas_->set_content(nullptr);

    auto props = std::make_unique<View>();
    {
      auto pb =
          std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
      pb->set_between_child_spacing(4);
      props->set_layout_manager(std::move(pb));
      props->set_preferred_size({220, 600});
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
    body->add_child(std::move(palette));
    body->add_child(std::move(canvas));
    body->add_child(std::move(props));
    if (auto* box = static_cast<BoxLayout*>(body->layout_manager())) {
      box->set_flex_for_view(canvas_view, 1);
    }
    View* body_raw = body.get();

    auto tree = std::make_unique<TreeView>();
    tree_ = tree.get();
    tree_->set_preferred_size({1200, 160});
    tree_->set_selection_changed([this](const TreeView::NodeId& id) {
      selected_id_ = id;
      sync_props();
      highlight();
    });

    auto status = std::make_unique<Label>("Ready");
    status_ = status.get();
    status_->set_preferred_size({1200, 22});

    set_layout_manager(std::move(outer));
    add_child(std::move(menu));
    add_child(std::move(body));
    add_child(std::move(tree));
    add_child(std::move(status));
    if (auto* box = static_cast<BoxLayout*>(layout_manager())) {
      box->set_flex_for_view(body_raw, 1);
    }
  }

  void open_path(const std::string& path) {
    path_ = path;
    reload(true);
  }

  void tick_hot_reload() {
    if (path_.empty() || suppress_) {
      return;
    }
    if (file_mtime(path_) != xml_mtime_ ||
        file_mtime(css_path_) != css_mtime_) {
      reload(false);
    }
  }

 private:
  static Textfield* add_prop(View* host, const char* caption) {
    auto lab = std::make_unique<Label>(caption);
    lab->set_preferred_size({200, 18});
    host->add_child(std::move(lab));
    auto tf = std::make_unique<Textfield>();
    tf->set_preferred_size({200, 26});
    Textfield* raw = tf.get();
    host->add_child(std::move(tf));
    return raw;
  }
  static void add_btn(View* host, const char* text, std::function<void()> fn) {
    auto btn = std::make_unique<Button>(text);
    btn->set_preferred_size({120, 28});
    btn->set_click(std::move(fn));
    host->add_child(std::move(btn));
  }

  void status_set(const std::string& s) {
    if (status_) {
      status_->set_text(s);
      status_->invalidate();
    }
  }

  void open_file() {
    HWND owner = widget() ? widget()->hwnd() : nullptr;
    auto r = ui::views::pick_open_file(
        owner, L"UI Markup (*.ui.xml)\0*.ui.xml\0All\0*.*\0");
    if (r.accepted) {
      open_path(r.path);
    }
  }

  void save_file(bool save_as) {
    if (save_as || path_.empty()) {
      HWND owner = widget() ? widget()->hwnd() : nullptr;
      auto r = ui::views::pick_save_file(
          owner, L"UI Markup (*.ui.xml)\0*.ui.xml\0All\0*.*\0");
      if (!r.accepted) {
        return;
      }
      path_ = r.path;
      css_path_ = path_;
      const size_t dot = css_path_.rfind(".ui.xml");
      if (dot != std::string::npos) {
        css_path_.replace(dot, 7, ".ui.css");
      } else {
        css_path_ += ".ui.css";
      }
    }
    if (!document_.save_files(path_, css_path_)) {
      status_set("save failed");
      return;
    }
    xml_mtime_ = file_mtime(path_);
    css_mtime_ = file_mtime(css_path_);
    status_set("saved " + path_);
  }

  void reload(bool announce) {
    suppress_ = true;
    clear_drag();
    const std::string keep = selected_id_;
    if (path_.empty()) {
      status_set("load failed: empty path");
      suppress_ = false;
      return;
    }

    // Resolve basename (e.g. preview_sample.ui.xml) to <exe>/ui/... once so
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
    css_path_ = path_;
    {
      const size_t dot = css_path_.rfind(".ui.xml");
      if (dot != std::string::npos) {
        css_path_.replace(dot, 7, ".ui.css");
      } else {
        css_path_ += ".ui.css";
      }
    }

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
      Size pref = built.root->get_preferred_size();
      if (pref.width <= 0) {
        pref.width = 320;
      }
      if (pref.height <= 0) {
        pref.height = 160;
      }
      built.root->set_preferred_size(pref);
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

  void rebuild_from_dom() {
    MarkupRoot out;
    MarkupOptions opt;
    if (!build_markup_tree(document_, opt, &out) || !out.ok()) {
      status_set("rebuild failed: " + out.error);
      return;
    }
    ids_ = std::move(out.ids);
    if (canvas_) {
      Size pref = out.root->get_preferred_size();
      if (pref.width <= 0) {
        pref.width = 320;
      }
      if (pref.height <= 0) {
        pref.height = 160;
      }
      out.root->set_preferred_size(pref);
      canvas_->set_content(std::move(out.root));
    }
    rebuild_tree();
    sync_props();
    highlight();
    mark_needs_layout();
    invalidate();
  }

  void rebuild_tree() {
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
            id = std::string(node.name()) + "_" +
                 std::to_string(++anon_seq_);
            node.append_attribute("id") = id.c_str();
          }
          tree_->add_node(parent, id,
                          std::string(node.name()) + " #" + id, false);
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

  std::string id_for_hit(View* hit) const {
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

  Rect view_bounds(View* v) const {
    if (!v) {
      return {};
    }
    // Views paint in widget space (see Label::paint_self).
    return v->bounds();
  }

  bool on_canvas_mouse(const MouseEvent& e) {
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

  void clear_drag() {
    drag_armed_ = false;
    dragging_ = false;
    drag_id_.clear();
    drop_before_id_.clear();
    drop_append_ = false;
    if (canvas_) {
      canvas_->set_drop_line({});
    }
  }

  struct SiblingSlot {
    std::string id;
    Rect bounds;
  };

  std::vector<SiblingSlot> sibling_slots(const std::string& moving_id) const {
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

  static bool siblings_use_horizontal(const std::vector<SiblingSlot>& slots) {
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

  void update_drop_target(int widget_x, int widget_y) {
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

  void apply_drop_reorder(const std::string& moving, const std::string& before_id,
                          bool append) {
    pugi::xml_node node =
        find_by_id(content_root(document_.raw_xml()), moving);
    if (!node || !node.parent()) {
      return;
    }
    pugi::xml_node parent = node.parent();
    if (append) {
      pugi::xml_node last;
      for (pugi::xml_node sib = parent.last_child(); sib;
           sib = sib.previous_sibling()) {
        if (sib.type() == pugi::node_element && sib != node) {
          last = sib;
          break;
        }
      }
      if (!last) {
        status_set("reorder noop");
        return;
      }
      pugi::xml_node after = node.next_sibling();
      while (after && after.type() != pugi::node_element) {
        after = after.next_sibling();
      }
      if (!after) {
        status_set("reorder noop");
        return;
      }
      parent.insert_move_after(node, last);
    } else {
      if (before_id.empty()) {
        return;
      }
      pugi::xml_node before =
          find_by_id(content_root(document_.raw_xml()), before_id);
      if (!before || before.parent() != parent) {
        status_set("drop target not sibling");
        return;
      }
      pugi::xml_node walk = node.next_sibling();
      while (walk && walk.type() != pugi::node_element) {
        walk = walk.next_sibling();
      }
      if (walk == before) {
        status_set("reorder noop");
        return;
      }
      parent.insert_move_before(node, before);
    }
    selected_id_ = moving;
    persist_and_rebuild("reordered");
  }

  void persist_and_rebuild(const std::string& status) {
    if (!path_.empty()) {
      document_.save_files(path_, css_path_);
      reload(true);
    } else {
      rebuild_from_dom();
    }
    status_set(status);
  }

  void highlight() {
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

  void sync_props() {
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

  void apply_properties() {
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

  void insert_pending() {
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

  void reorder_selected(int delta) {
    pugi::xml_node node =
        find_by_id(content_root(document_.raw_xml()), selected_id_);
    if (!node || !node.parent()) {
      return;
    }
    pugi::xml_node parent = node.parent();
    if (delta < 0) {
      pugi::xml_node prev = node.previous_sibling();
      while (prev && prev.type() != pugi::node_element) {
        prev = prev.previous_sibling();
      }
      if (prev) {
        parent.insert_move_before(node, prev);
      }
    } else {
      pugi::xml_node next = node.next_sibling();
      while (next && next.type() != pugi::node_element) {
        next = next.next_sibling();
      }
      if (next) {
        parent.insert_move_after(node, next);
      }
    }
    persist_and_rebuild("reordered");
  }

  std::string path_;
  std::string css_path_;
  uint64_t xml_mtime_ = 0;
  uint64_t css_mtime_ = 0;
  bool suppress_ = false;
  MarkupDocument document_;
  NamedViewMap ids_;
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
  TreeView* tree_ = nullptr;
  Label* status_ = nullptr;
  Textfield* prop_id_ = nullptr;
  Textfield* prop_text_ = nullptr;
  Textfield* prop_class_ = nullptr;
  Textfield* prop_width_ = nullptr;
  Textfield* prop_height_ = nullptr;
  Textfield* prop_flex_ = nullptr;
};

DesignerShell* g_shell = nullptr;

VOID CALLBACK hot_reload_timer(HWND, UINT, UINT_PTR, DWORD) {
  if (g_shell) {
    g_shell->tick_hot_reload();
  }
}

}  // namespace

int run_ui_designer(const std::string& initial_path) {
  // Also invoked inside Widget::init; call early so Theme::current() is ready.
  ThemeService::get().ensure_builtin_packs();
  ThemeService::get().load_persisted();

  Widget widget;
  Widget::InitParams params;
  params.title = L"UiDesigner";
  params.width = 1280;
  params.height = 800;
  params.size_in_dips = true;
  params.frame_kind = Widget::FrameKind::kCustom;
  if (!widget.init(params)) {
    return 1;
  }
  auto shell = std::make_unique<DesignerShell>();
  shell->build_ui();
  g_shell = shell.get();
  DesignerShell* raw = shell.get();
  auto frame = std::make_unique<FrameView>();
  frame->set_title("UiDesigner");
  frame->set_can_maximize(true);
  frame->set_client(std::move(shell));
  widget.set_contents_view(std::move(frame));
  raw->open_path(initial_path.empty() ? "preview_sample.ui.xml"
                                      : initial_path);
  SetTimer(widget.hwnd(), 1, 500, hot_reload_timer);
  widget.show();
  const int rc = widget.run_loop();
  KillTimer(widget.hwnd(), 1);
  g_shell = nullptr;
  return rc;
}

}  // namespace app
