// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/panels/plugin_catalog_view.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/views/app/startup/scenario.h"
#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/core/log.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "tool/command/command.h"
#include "ui/gis/scroll_table.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace app {
namespace {

// Copy Registry::last_error via C string — do not assign std::string across
// the EXE↔plugin_host Debug STL boundary.
std::string registry_error_text(plugin::Registry* registry) {
  if (!registry) {
    return {};
  }
  const char* err = registry->last_error_cstr();
  return (err && err[0]) ? std::string(err) : std::string();
}

const char* state_name(plugin::PluginState s) {
  switch (s) {
    case plugin::PluginState::kDisabled:
      return "off";
    case plugin::PluginState::kEnabled:
      return "on";
    case plugin::PluginState::kError:
      return "error";
    case plugin::PluginState::kInvalidExports:
      return "invalid";
    case plugin::PluginState::kLoadFailed:
      return "failed";
  }
  return "?";
}

bool contains_text(std::string_view hay, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  if (hay.find(needle) != std::string_view::npos) {
    return true;
  }
  std::string h(hay);
  std::string n(needle);
  const auto lower = [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  };
  std::transform(h.begin(), h.end(), h.begin(), lower);
  std::transform(n.begin(), n.end(), n.begin(), lower);
  return h.find(n) != std::string::npos;
}

ui::views::Button* find_commit_button(ui::views::View* view) {
  if (!view) {
    return nullptr;
  }
  if (auto* button = dynamic_cast<ui::views::Button*>(view)) {
    const std::string& text = button->text();
    if (text == "OK" || text == "Run" || text == "确定" || text == "执行") {
      return button;
    }
  }
  for (size_t i = 0; i < view->child_count(); ++i) {
    if (ui::views::Button* found = find_commit_button(view->child_at(i))) {
      return found;
    }
  }
  return nullptr;
}

ui::views::Label* add_caption(ui::views::View* parent, const char* text) {
  const ui::views::Theme& theme = ui::views::Theme::current();
  auto label = std::make_unique<ui::views::Label>(text);
  ui::views::Label* raw = label.get();
  raw->set_color(theme.text_muted);
  raw->set_preferred_size({264, 16});
  parent->add_child(std::move(label));
  return raw;
}

}  // namespace

PluginCatalogView::PluginCatalogView(plugin::Registry* registry,
                                     content::PluginHost* host,
                                     Browser* browser)
    : registry_(registry), host_(host), browser_(browser) {
  auto layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(4);
  layout->set_inside_border(8);
  ui::views::BoxLayout* box = layout.get();
  set_layout_manager(std::move(layout));

  const ui::views::Theme& theme = ui::views::Theme::current();

  auto search = std::make_unique<ui::views::Textfield>();
  search_ = search.get();
  search_->set_placeholder("Filter plugins and tools");
  search_->set_change([this]() { on_search(); });
  search_->set_preferred_size({264, 24});
  add_child(std::move(search));

  add_caption(this, "Plugins");
  auto plugins = std::make_unique<ui::views::TableView>();
  plugins_ = plugins.get();
  // One column: two-column tables give the first column only ~42% width.
  plugins_->set_columns({"plugin"});
  plugins_->set_row_click([this](int row) { on_plugin_click(row); });
  plugins_->set_cell_activate(
      [this](int row, int col) { on_plugin_activate(row, col); });
  plugins_->set_preferred_size({264, 96});
  add_child(std::move(plugins));
  plugin_scroll_ = ui::views::wrap_markup_table_in_scroll(plugins_, 96.f);
  if (plugin_scroll_) {
    plugin_scroll_->set_preferred_size({264, 96});
  }

  auto actions = std::make_unique<ui::views::View>();
  auto actions_layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kHorizontal);
  actions_layout->set_between_child_spacing(6);
  ui::views::BoxLayout* actions_box = actions_layout.get();
  actions->set_layout_manager(std::move(actions_layout));

  auto enable = std::make_unique<ui::views::Button>("Enable");
  enable->set_click([this]() { on_enable(); });
  actions_box->set_flex_for_view(enable.get(), 1);
  actions->add_child(std::move(enable));

  auto disable = std::make_unique<ui::views::Button>("Disable");
  disable->set_click([this]() { on_disable(); });
  actions_box->set_flex_for_view(disable.get(), 1);
  actions->add_child(std::move(disable));

  auto run = std::make_unique<ui::views::Button>("Run");
  run->set_style(ui::views::Button::Style::kPrimary);
  run->set_click([this]() { on_run(); });
  actions_box->set_flex_for_view(run.get(), 1);
  actions->add_child(std::move(run));

  actions->set_preferred_size({264, 28});
  add_child(std::move(actions));

  auto commands = std::make_unique<ui::views::View>();
  auto commands_layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kHorizontal);
  commands_layout->set_between_child_spacing(4);
  commands_box_ = commands_layout.get();
  commands->set_layout_manager(std::move(commands_layout));
  commands_ = commands.get();
  commands_->set_preferred_size({264, 28});
  auto commands_scroll = std::make_unique<ui::views::ScrollView>();
  commands_scroll_ = commands_scroll.get();
  commands_scroll_->add_child(std::move(commands));
  // Long tool lists scroll here so the form below keeps a viewport.
  commands_scroll_->set_preferred_size({264, 28});
  add_child(std::move(commands_scroll));

  auto form_scroll = std::make_unique<ui::views::ScrollView>();
  form_scroll_ = form_scroll.get();
  auto form = std::make_unique<ui::views::View>();
  auto form_layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  form_layout->set_between_child_spacing(4);
  form_box_ = form_layout.get();
  form->set_layout_manager(std::move(form_layout));
  form_ = form.get();
  form_scroll_->add_child(std::move(form));
  form_scroll_->set_preferred_size({264, 220});
  box->set_flex_for_view(form_scroll_, 1);
  add_child(std::move(form_scroll));

  auto detail = std::make_unique<ui::views::Label>(
      "Select a plugin. Its commands open here.");
  detail_ = detail.get();
  detail_->set_color(theme.text_muted);
  detail_->set_preferred_size({264, 48});
  add_child(std::move(detail));

  auto err = std::make_unique<ui::views::Label>("");
  error_ = err.get();
  error_->set_color(theme.text_muted);
  error_->set_preferred_size({264, 20});
  add_child(std::move(err));

  set_preferred_size({280, 420});
  plugin::set_dialog_presenter(this, [](void* context, const wchar_t* title,
                                        std::unique_ptr<ui::views::View>* body) {
    static_cast<PluginCatalogView*>(context)->accept_presented(title, body);
  });
  refresh();
}

PluginCatalogView::~PluginCatalogView() {
  plugin::set_dialog_presenter(this, nullptr);
}

bool PluginCatalogView::run_modal(HWND owner, plugin::Registry* registry,
                                  content::PluginHost* host,
                                  Browser* browser) {
  if (!registry || !host) {
    return false;
  }
  ui::views::Dialog::run_modal(
      owner, L"Plugins", 720, 520,
      std::make_unique<PluginCatalogView>(registry, host, browser));
  return true;
}

bool PluginCatalogView::is_about_tool(const ToolEntry& tool) {
  if (tool.id.size() >= 6 &&
      tool.id.compare(tool.id.size() - 6, 6, ".about") == 0) {
    return true;
  }
  return tool.title == "关于" || tool.title == "About" || tool.title == "about";
}

std::string PluginCatalogView::query_text() const {
  return search_ ? search_->text() : std::string();
}

bool PluginCatalogView::record_matches(const std::string& id,
                                       const std::string& name,
                                       const std::string& description) const {
  const std::string q = query_text();
  if (q.empty()) {
    return true;
  }
  if (contains_text(id, q) || contains_text(name, q) ||
      contains_text(description, q)) {
    return true;
  }
  if (!registry_) {
    return false;
  }
  struct MatchCtx {
    const std::string* query;
    bool hit;
  } ctx{&q, false};
  registry_->visit_tools(
      id,
      [](const char* /*kind*/, const char* tool_id, const char* title,
         void* baton) {
        auto* c = static_cast<MatchCtx*>(baton);
        if (c->hit || !c->query) {
          return;
        }
        if (contains_text(tool_id ? tool_id : "", *c->query) ||
            contains_text(title ? title : "", *c->query)) {
          c->hit = true;
        }
      },
      &ctx);
  return ctx.hit;
}

void PluginCatalogView::refresh() {
  fill_plugins(true);
  if (selected_tool_ >= 0 && plugin_enabled(selected_plugin_id_)) {
    mount_body();
  } else if (selected_tool_ < 0) {
    clear_form();
  }
}

void PluginCatalogView::on_search() {
  refresh();
}

void PluginCatalogView::fill_plugins(bool rebuild_tools) {
  if (!plugins_) {
    return;
  }
  plugins_->clear_rows();
  visible_plugin_ids_.clear();
  if (!registry_) {
    if (rebuild_tools) {
      fill_tools();
    }
    return;
  }
  struct FillCtx {
    PluginCatalogView* self;
    int restore;
  } ctx{this, -1};
  registry_->visit(
      [](const char* id, const char* name, int state, int /*kind*/, int /*trust*/,
         void* baton) {
        auto* c = static_cast<FillCtx*>(baton);
        if (!c || !c->self || !id || !id[0]) {
          return;
        }
        std::string desc;
        c->self->registry_->visit_detail(
            id,
            [](const char* /*pid*/, const char* /*pname*/, const char* /*ver*/,
               const char* description, int /*st*/, int /*handlers*/,
               void* db) {
              auto* out = static_cast<std::string*>(db);
              if (out && description) {
                *out = description;
              }
            },
            &desc);
        const std::string id_s(id);
        const std::string name_s = (name && name[0]) ? std::string(name) : id_s;
        if (!c->self->record_matches(id_s, name_s, desc)) {
          return;
        }
        if (id_s == c->self->selected_plugin_id_) {
          c->restore = static_cast<int>(c->self->visible_plugin_ids_.size());
        }
        c->self->visible_plugin_ids_.push_back(id_s);
        c->self->plugins_->add_row(
            {std::string(state_name(static_cast<plugin::PluginState>(state))) +
             "   " + name_s});
      },
      &ctx);
  if (ctx.restore < 0) {
    selected_plugin_id_.clear();
    selected_tool_ = -1;
  } else {
    plugins_->set_selected_row(ctx.restore);
  }
  ui::views::sync_scroll_table_content(plugins_, plugin_scroll_, 264);
  if (rebuild_tools) {
    fill_tools();
    fill_detail();
  }
}

void PluginCatalogView::collect_tools(const std::string& plugin_id) {
  tool_entries_.clear();
  if (!registry_ || plugin_id.empty()) {
    return;
  }
  std::string plugin_name;
  std::string plugin_desc;
  bool found = false;
  struct DetailCtx {
    std::string* name;
    std::string* desc;
    bool* found;
  } detail{&plugin_name, &plugin_desc, &found};
  registry_->visit_detail(
      plugin_id,
      [](const char* /*id*/, const char* name, const char* /*ver*/,
         const char* description, int /*state*/, int /*handlers*/, void* baton) {
        auto* c = static_cast<DetailCtx*>(baton);
        if (!c) {
          return;
        }
        if (c->found) {
          *c->found = true;
        }
        if (c->name && name) {
          *c->name = name;
        }
        if (c->desc && description) {
          *c->desc = description;
        }
      },
      &detail);
  if (!found) {
    return;
  }
  const std::string q = query_text();
  const bool plugin_hit = q.empty() || contains_text(plugin_id, q) ||
                          contains_text(plugin_name, q) ||
                          contains_text(plugin_desc, q);
  std::vector<std::string> seen;
  // Prefer dialog/dock/processing over a bare command when the same id is
  // listed in several contribute sections (flood.inundate, etc.). Order is
  // owned by Registry::visit_tools.
  struct ToolCtx {
    std::vector<ToolEntry>* out;
    std::vector<std::string>* seen;
    const std::string* q;
    bool plugin_hit;
  } tool_ctx{&tool_entries_, &seen, &q, plugin_hit};
  registry_->visit_tools(
      plugin_id,
      [](const char* kind, const char* id, const char* title, void* baton) {
        auto* c = static_cast<ToolCtx*>(baton);
        if (!c || !c->out || !c->seen || !id || !id[0]) {
          return;
        }
        const std::string id_s(id);
        const std::string title_s =
            (title && title[0]) ? std::string(title) : id_s;
        const std::string empty;
        const std::string& query = c->q ? *c->q : empty;
        if (!c->plugin_hit && !contains_text(id_s, query) &&
            !contains_text(title_s, query)) {
          return;
        }
        if (std::find(c->seen->begin(), c->seen->end(), id_s) !=
            c->seen->end()) {
          return;
        }
        c->seen->push_back(id_s);
        ToolEntry entry;
        if (kind && std::strcmp(kind, "dialog") == 0) {
          entry.kind = ToolKind::kDialog;
        } else if (kind && std::strcmp(kind, "dock") == 0) {
          entry.kind = ToolKind::kDock;
        } else if (kind && std::strcmp(kind, "processing") == 0) {
          entry.kind = ToolKind::kProcessing;
        } else {
          entry.kind = ToolKind::kCommand;
        }
        entry.id = id_s;
        entry.title = title_s;
        c->out->push_back(std::move(entry));
      },
      &tool_ctx);
}

void PluginCatalogView::fill_tools() {
  if (!commands_ || !commands_box_) {
    return;
  }
  const std::string keep_id =
      (selected_tool_ >= 0 &&
       selected_tool_ < static_cast<int>(tool_entries_.size()))
          ? tool_entries_[static_cast<size_t>(selected_tool_)].id
          : std::string();
  collect_tools(selected_plugin_id_);
  command_buttons_.clear();
  commands_->remove_all_children();
  const bool row = tool_entries_.size() <= 3;
  // Recreate the strip so a long command list stacks instead of clipping.
  auto layout = std::make_unique<ui::views::BoxLayout>(
      row ? ui::views::BoxLayout::Orientation::kHorizontal
          : ui::views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(4);
  commands_box_ = layout.get();
  commands_->set_layout_manager(std::move(layout));
  int restore = -1;
  for (size_t i = 0; i < tool_entries_.size(); ++i) {
    const ToolEntry& tool = tool_entries_[i];
    if (!keep_id.empty() && tool.id == keep_id) {
      restore = static_cast<int>(i);
    }
    auto button = std::make_unique<ui::views::Button>(tool.title);
    ui::views::Button* raw = button.get();
    const int index = static_cast<int>(i);
    raw->set_click([this, index]() { show_tool(index); });
    raw->set_preferred_size({row ? 80 : 248, 26});
    commands_box_->set_flex_for_view(raw, 1);
    command_buttons_.push_back(raw);
    commands_->add_child(std::move(button));
  }
  selected_tool_ = restore;
  const int rows = row ? 1 : static_cast<int>(tool_entries_.size());
  const int height = tool_entries_.empty() ? 0 : rows * 26 + (rows - 1) * 4;
  commands_->set_preferred_size({264, height});
  // Four rows, then the strip scrolls. Uncapped stacks shrink the form to 0.
  constexpr int kCommandStripCap = 4 * 26 + 3 * 4;
  const int shown = height <= 0 ? 0 : (height < kCommandStripCap ? height
                                                                 : kCommandStripCap);
  if (commands_scroll_) {
    commands_scroll_->set_preferred_size({264, shown});
  }
  style_command_buttons();
}

void PluginCatalogView::fill_detail() {
  if (!detail_) {
    return;
  }
  if (!registry_ || selected_plugin_id_.empty()) {
    detail_->set_text("Select a plugin. Its commands open here.");
    return;
  }
  struct DetailCtx {
    PluginCatalogView* self;
    bool ok;
  } ctx{this, false};
  registry_->visit_detail(
      selected_plugin_id_,
      [](const char* id, const char* name, const char* version,
         const char* description, int state, int /*handlers*/, void* baton) {
        auto* c = static_cast<DetailCtx*>(baton);
        if (!c || !c->self || !c->self->detail_ || !id) {
          return;
        }
        c->ok = true;
        std::string text = (name && name[0]) ? std::string(name) : std::string(id);
        text += "  v";
        text += (version && version[0]) ? version : "?";
        text += "  ";
        text += state_name(static_cast<plugin::PluginState>(state));
        if (description && description[0]) {
          text += "  ";
          text += description;
        }
        if (c->self->tool_entries_.empty()) {
          text += "  No tools in plugin.json.";
        }
        c->self->detail_->set_text(text);
      },
      &ctx);
  if (!ctx.ok) {
    detail_->set_text("Select a plugin. Its commands open here.");
  }
}

std::string PluginCatalogView::selected_id() const {
  if (!selected_plugin_id_.empty()) {
    return selected_plugin_id_;
  }
  if (!plugins_ || plugins_->selected_row() < 0) {
    return {};
  }
  const int row = plugins_->selected_row();
  if (row < 0 || row >= static_cast<int>(visible_plugin_ids_.size())) {
    return {};
  }
  return visible_plugin_ids_[static_cast<size_t>(row)];
}

void PluginCatalogView::on_plugin_click(int row) {
  if (row < 0 || row >= static_cast<int>(visible_plugin_ids_.size())) {
    return;
  }
  selected_plugin_id_ = visible_plugin_ids_[static_cast<size_t>(row)];
  selected_tool_ = -1;
  LOGGING(LOG_INFO, "ui.plugin_catalog: select %s", selected_plugin_id_.c_str());
  fill_tools();
  fill_detail();
  LOGGING(LOG_INFO, "ui.plugin_catalog: tools=%d",
          static_cast<int>(tool_entries_.size()));
  if (error_) {
    error_->set_text("");
  }
  const int index = primary_tool_index();
  if (index >= 0) {
    show_tool(index);
  } else {
    clear_form();
  }
}

void PluginCatalogView::on_plugin_activate(int row, int /*col*/) {
  on_plugin_click(row);
  if (!registry_ || selected_plugin_id_.empty()) {
    return;
  }
  struct StateCtx {
    int state;
    int handlers;
    bool ok;
  } st{-1, 0, false};
  registry_->visit_detail(
      selected_plugin_id_,
      [](const char* /*id*/, const char* /*name*/, const char* /*ver*/,
         const char* /*desc*/, int state, int handlers, void* baton) {
        auto* c = static_cast<StateCtx*>(baton);
        if (!c) {
          return;
        }
        c->ok = true;
        c->state = state;
        c->handlers = handlers;
      },
      &st);
  if (!st.ok) {
    return;
  }
  if (st.state == static_cast<int>(plugin::PluginState::kEnabled) &&
      st.handlers) {
    on_disable();
  } else {
    on_enable();
  }
}

void PluginCatalogView::on_enable() {
  if (!registry_ || !host_) {
    return;
  }
  const std::string id = selected_id();
  if (id.empty()) {
    if (error_) {
      error_->set_text("Select a plugin");
    }
    return;
  }
  if (!registry_->set_enabled(id, true, host_)) {
    if (error_) {
      const std::string err = registry_error_text(registry_);
      error_->set_text(err.empty() ? "Enable failed" : err);
    }
    fill_plugins(true);
    return;
  }
  if (error_) {
    error_->set_text("Enabled " + id);
  }
  fill_plugins(false);
  if (selected_tool_ < 0) {
    selected_tool_ = primary_tool_index();
    style_command_buttons();
  }
  if (selected_tool_ >= 0) {
    mount_body();
  }
}

void PluginCatalogView::on_disable() {
  if (!registry_ || !host_) {
    return;
  }
  const std::string id = selected_id();
  if (id.empty()) {
    if (error_) {
      error_->set_text("Select a plugin");
    }
    return;
  }
  (void)registry_->set_enabled(id, false, host_);
  if (error_) {
    error_->set_text("Disabled " + id);
  }
  fill_plugins(false);
  clear_form();
}

bool PluginCatalogView::ensure_enabled(const std::string& plugin_id) {
  if (!registry_ || !host_) {
    if (error_) {
      error_->set_text("Run needs the plugin host");
    }
    return false;
  }
  struct StateCtx {
    int state;
    int handlers;
    bool ok;
  } st{-1, 0, false};
  registry_->visit_detail(
      plugin_id,
      [](const char* /*id*/, const char* /*name*/, const char* /*ver*/,
         const char* /*desc*/, int state, int handlers, void* baton) {
        auto* c = static_cast<StateCtx*>(baton);
        if (!c) {
          return;
        }
        c->ok = true;
        c->state = state;
        c->handlers = handlers;
      },
      &st);
  if (st.ok && st.state == static_cast<int>(plugin::PluginState::kEnabled) &&
      st.handlers) {
    return true;
  }
  if (!registry_->set_enabled(plugin_id, true, host_)) {
    if (error_) {
      const std::string err = registry_error_text(registry_);
      error_->set_text(err.empty() ? "Enable failed" : err);
    }
    fill_plugins(false);
    return false;
  }
  fill_plugins(false);
  if (error_) {
    error_->set_text("Enabled " + plugin_id);
  }
  return true;
}

bool PluginCatalogView::execute_tool(const std::string& plugin_id,
                                     const ToolEntry& tool) {
  if (!host_) {
    return false;
  }
  PluginShell* shell = browser_ ? browser_->plugins() : nullptr;
  if (shell) {
    (void)shell->ensure_command(tool.id);
  } else {
    (void)plugin::ensure_for_command(host_, tool.id);
  }
  bool ok = false;
  switch (tool.kind) {
    case ToolKind::kCommand:
      ok = shell ? shell->execute(tool.id, {}) : host_->execute(tool.id, {});
      if (!ok) {
        ok = host_->open_dialog(tool.id);
      }
      break;
    case ToolKind::kProcessing:
      ok = shell ? shell->run_processing(tool.id, "{}")
                 : host_->run_processing(tool.id, "{}");
      break;
    case ToolKind::kDialog:
      ok = host_->open_dialog(tool.id);
      break;
    case ToolKind::kDock:
      ok = host_->open_dock(tool.id);
      break;
  }
  if (error_) {
    if (ok) {
      error_->set_text("Ran " + tool.title);
    } else {
      error_->set_text("Run failed: " + tool.title + " (" + plugin_id + ")");
    }
  }
  return ok;
}

int PluginCatalogView::primary_tool_index() const {
  int fallback = -1;
  for (size_t i = 0; i < tool_entries_.size(); ++i) {
    const ToolEntry& tool = tool_entries_[i];
    if (tool.kind != ToolKind::kCommand && tool.kind != ToolKind::kDialog &&
        tool.kind != ToolKind::kDock) {
      if (fallback < 0) {
        fallback = static_cast<int>(i);
      }
      continue;
    }
    if (is_about_tool(tool)) {
      continue;
    }
    if (tool.kind == ToolKind::kCommand || tool.kind == ToolKind::kDialog ||
        tool.kind == ToolKind::kDock) {
      return static_cast<int>(i);
    }
  }
  return fallback >= 0 ? fallback : (tool_entries_.empty() ? -1 : 0);
}

void PluginCatalogView::run_scenario_fallback(const std::string& plugin_id) {
  if (!browser_) {
    if (error_) {
      error_->set_text("No tools for " + plugin_id);
    }
    return;
  }
  ensure_builtin_scenarios();
  std::string sid;
  if (find_scenario(plugin_id)) {
    sid = plugin_id;
  } else if (find_scenario("plugin." + plugin_id)) {
    sid = "plugin." + plugin_id;
  } else if (plugin_id == "map2d" && find_scenario("browser.map2d.china")) {
    sid = "browser.map2d.china";
  }
  if (sid.empty()) {
    if (error_) {
      error_->set_text("No tools for " + plugin_id);
    }
    return;
  }
  const Scenario* scenario = find_scenario(sid);
  if (!scenario) {
    if (error_) {
      error_->set_text("Unknown scenario " + sid);
    }
    return;
  }
  const int rc = run_scenario(*browser_, *scenario);
  if (error_) {
    error_->set_text(rc == 0 ? "Run OK: " + sid
                             : "Run failed (" + std::to_string(rc) + "): " + sid);
  }
}

void PluginCatalogView::on_run() {
  if (ui::views::Button* commit = find_commit_button(form_)) {
    commit->click();
    return;
  }
  const std::string plugin_id = selected_id();
  LOGGING(LOG_INFO, "ui.plugin_catalog: run plugin=%s tool=%d",
          plugin_id.c_str(), selected_tool_);
  if (plugin_id.empty()) {
    if (error_) {
      error_->set_text("Select a plugin");
    }
    return;
  }
  if (!ensure_enabled(plugin_id)) {
    return;
  }
  if (selected_tool_ < 0 ||
      selected_tool_ >= static_cast<int>(tool_entries_.size())) {
    selected_tool_ = primary_tool_index();
  }
  if (selected_tool_ < 0) {
    run_scenario_fallback(plugin_id);
    return;
  }
  const ToolEntry& tool = tool_entries_[static_cast<size_t>(selected_tool_)];
  const bool ok = execute_tool(plugin_id, tool);
  if (presented_) {
    return;
  }
  if (error_) {
    error_->set_text(ok ? "Ran " + tool.title : "Run failed: " + tool.title);
  }
}

void PluginCatalogView::show_tool(int index) {
  if (index < 0 || index >= static_cast<int>(tool_entries_.size())) {
    return;
  }
  selected_tool_ = index;
  style_command_buttons();
  if (!ensure_enabled(selected_plugin_id_)) {
    return;
  }
  mount_body();
}

void PluginCatalogView::style_command_buttons() {
  for (size_t i = 0; i < command_buttons_.size(); ++i) {
    ui::views::Button* button = command_buttons_[i];
    if (!button) {
      continue;
    }
    button->set_style(static_cast<int>(i) == selected_tool_
                          ? ui::views::Button::Style::kPrimary
                          : ui::views::Button::Style::kDefault);
  }
}

void PluginCatalogView::clear_form() {
  presented_ = false;
  if (form_) {
    form_->remove_all_children();
  }
}

void PluginCatalogView::accept_presented(
    const wchar_t* /*title*/,
    std::unique_ptr<ui::views::View>* body) {
  if (!form_ || !form_box_ || !body || !*body) {
    return;
  }
  presented_ = true;
  auto taken = std::move(*body);
  ui::views::Size pref = taken->preferred_size();
  int width = 248;
  if (form_scroll_ && form_scroll_->bounds().width > 64) {
    width = form_scroll_->bounds().width;
  }
  if (pref.height <= 0) {
    pref.height = 280;
  }
  // Keep the dialog's own height so the scroll view can reach OK. Flex-grow
  // shrinks that height into the dock and clips the lower fields.
  taken->set_preferred_size({width, pref.height});
  form_->remove_all_children();
  form_box_->set_flex_for_view(taken.get(), 0);
  form_->add_child(std::move(taken));
  if (form_scroll_) {
    form_scroll_->set_scroll_offset(0);
  }
}

bool PluginCatalogView::plugin_enabled(const std::string& plugin_id) const {
  if (!registry_ || plugin_id.empty()) {
    return false;
  }
  struct StateCtx {
    int state;
    bool ok;
  } st{-1, false};
  registry_->visit_detail(
      plugin_id,
      [](const char* /*id*/, const char* /*name*/, const char* /*ver*/,
         const char* /*desc*/, int state, int /*handlers*/, void* baton) {
        auto* c = static_cast<StateCtx*>(baton);
        if (!c) {
          return;
        }
        c->ok = true;
        c->state = state;
      },
      &st);
  return st.ok && st.state == static_cast<int>(plugin::PluginState::kEnabled);
}

bool PluginCatalogView::dialog_registered(const std::string& tool_id) const {
  if (!host_ || tool_id.empty()) {
    return false;
  }
  bool found = false;
  host_->for_each_dialog(
      [&](std::string_view /*plugin_id*/, std::string_view dialog_id,
          std::string_view /*title*/) {
        if (dialog_id == tool_id) {
          found = true;
        }
      });
  return found;
}

bool PluginCatalogView::processing_registered(const std::string& tool_id) const {
  if (!host_ || tool_id.empty()) {
    return false;
  }
  bool found = false;
  host_->for_each_processing(
      [&](std::string_view /*plugin_id*/, std::string_view processing_id,
          std::string_view /*title*/) {
        if (processing_id == tool_id) {
          found = true;
        }
      });
  return found;
}

void PluginCatalogView::show_run_card(const ToolEntry& tool) {
  if (!form_ || !form_box_) {
    return;
  }
  form_->remove_all_children();
  auto card = std::make_unique<ui::views::View>();
  auto layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(6);
  ui::views::BoxLayout* box = layout.get();
  card->set_layout_manager(std::move(layout));

  auto title = std::make_unique<ui::views::Label>(tool.title);
  title->set_preferred_size({248, 18});
  card->add_child(std::move(title));

  auto run = std::make_unique<ui::views::Button>("Run");
  run->set_style(ui::views::Button::Style::kPrimary);
  run->set_preferred_size({248, 28});
  const std::string plugin_id = selected_plugin_id_;
  const ToolKind kind = tool.kind;
  const std::string id = tool.id;
  run->set_click([this, plugin_id, kind, id]() {
    if (!host_) {
      return;
    }
    bool ok = false;
    if (kind == ToolKind::kProcessing) {
      ok = host_->run_processing(id, "{}");
    } else {
      ToolEntry entry;
      entry.kind = kind;
      entry.id = id;
      entry.title = id;
      ok = execute_tool(plugin_id, entry);
    }
    if (error_) {
      error_->set_text(ok ? "Ran " + id : "Run failed: " + id);
    }
  });
  box->set_flex_for_view(run.get(), 0);
  card->add_child(std::move(run));
  card->set_preferred_size({248, 64});
  form_box_->set_flex_for_view(card.get(), 1);
  form_->add_child(std::move(card));
}

void PluginCatalogView::mount_body() {
  if (selected_tool_ < 0 ||
      selected_tool_ >= static_cast<int>(tool_entries_.size())) {
    clear_form();
    if (!tool_entries_.empty()) {
      return;
    }
    if (!selected_plugin_id_.empty()) {
      run_scenario_fallback(selected_plugin_id_);
    }
    return;
  }
  const ToolEntry tool = tool_entries_[static_cast<size_t>(selected_tool_)];
  presented_ = false;
  clear_form();
  if (!host_) {
    show_run_card(tool);
    return;
  }
  PluginShell* shell = browser_ ? browser_->plugins() : nullptr;
  if (shell) {
    (void)shell->ensure_command(tool.id);
  } else {
    (void)plugin::ensure_for_command(host_, tool.id);
  }
  bool opened = false;
  if (tool.kind == ToolKind::kDock) {
    opened = host_->open_dock(tool.id);
  } else if (tool.kind == ToolKind::kDialog || dialog_registered(tool.id)) {
    opened = host_->open_dialog(tool.id);
  } else if (tool.kind == ToolKind::kProcessing ||
             processing_registered(tool.id)) {
    // Parameter card + Run; do not auto-fire processing with "{}".
  } else if (tool.kind == ToolKind::kCommand) {
    opened = execute_tool(selected_plugin_id_, tool);
  }
  if (presented_) {
    return;
  }
  if (tool.kind == ToolKind::kDock || tool.kind == ToolKind::kDialog ||
      dialog_registered(tool.id) || tool.kind == ToolKind::kCommand) {
    if (!opened && error_ && tool.kind != ToolKind::kProcessing &&
        !processing_registered(tool.id)) {
      error_->set_text("Open failed: " + tool.title);
    }
  }
  show_run_card(tool);
}

}  // namespace app
