// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace content {
namespace {

struct CommandRec {
  std::string plugin_id;
  std::string command_id;
  tool::CommandHandler handler;
};

struct DialogRec {
  std::string plugin_id;
  DialogContribution dialog;
  DialogFactory factory;
};

struct ProcessingRec {
  std::string plugin_id;
  ProcessingContribution proc;
  ProcessingFactory factory;
};

struct MenuRec {
  std::string plugin_id;
  MenuContribution menu;
};

struct DockRec {
  std::string plugin_id;
  DockContribution dock;
  DialogFactory factory;
};

struct PainterRec {
  std::string plugin_id;
  std::string role;
};

struct ExportFrameRec {
  std::string plugin_id;
  ExportFrameContribution frame;
};

class PluginHostImpl final : public PluginHost {
 public:
  PluginHostImpl(tool::CommandCatalog* catalog,
                 EventBus* events,
                 MapContents* maps)
      : catalog_(catalog), events_(events), maps_(maps) {}

  MapContents* map_contents() override { return maps_; }
  EventBus* events() override { return events_; }
  tool::CommandCatalog* commands() override { return catalog_; }

  void set_processing_enqueue(ProcessingEnqueue fn) override {
    enqueue_ = std::move(fn);
  }

  void set_ui_withdraw_hook(UiWithdrawHook hook) override {
    ui_withdraw_hook_ = std::move(hook);
  }

  bool contribute_command(std::string_view plugin_id,
                          std::string_view command_id,
                          std::string_view title,
                          std::string_view menu_id,
                          tool::CommandHandler handler) override {
    (void)menu_id;
    if (plugin_id.empty() || command_id.empty() || !handler) {
      return false;
    }
    const std::string cid(command_id);
    if (handlers_.contains(cid) && !withdrawn_commands_.contains(cid)) {
      return false;
    }
    // Catalog and handlers_ both own a callable — copy into the catalog first
    // so |handler| is not left empty for PluginHost::execute.
    if (catalog_ && !catalog_->contains(cid)) {
      if (!catalog_->add(cid, tool::CommandHandler{handler})) {
        return false;
      }
    }
    withdrawn_commands_.erase(cid);
    handlers_[cid] = std::move(handler);
    command_owners_[cid] = std::string(plugin_id);
    command_titles_[cid] = std::string(title);
    return true;
  }

  bool contribute_menu(std::string_view plugin_id,
                       const MenuContribution& menu) override {
    if (plugin_id.empty() || menu.id.empty()) {
      return false;
    }
    menus_.push_back({std::string(plugin_id), menu});
    return true;
  }

  bool contribute_dock(std::string_view plugin_id,
                       const DockContribution& dock,
                       DialogFactory factory) override {
    if (plugin_id.empty() || dock.id.empty() || !factory) {
      return false;
    }
    docks_.push_back({std::string(plugin_id), dock, std::move(factory)});
    return true;
  }

  bool contribute_dialog(std::string_view plugin_id,
                         const DialogContribution& dialog,
                         DialogFactory factory) override {
    if (plugin_id.empty() || dialog.id.empty() || !factory) {
      return false;
    }
    dialogs_[dialog.id] = {std::string(plugin_id), dialog, std::move(factory)};
    return true;
  }

  bool contribute_processing(std::string_view plugin_id,
                             const ProcessingContribution& proc,
                             ProcessingFactory factory) override {
    if (plugin_id.empty() || proc.id.empty() || !factory) {
      return false;
    }
    processing_[proc.id] = {std::string(plugin_id), proc, std::move(factory)};
    return true;
  }

  bool contribute_painter(std::string_view plugin_id,
                          std::string_view role,
                          UiPainterInstaller install) override {
    if (plugin_id.empty() || role.empty() || !install) {
      return false;
    }
    install();
    painters_.push_back({std::string(plugin_id), std::string(role)});
    return true;
  }

  bool execute(std::string_view command_id,
               const tool::CommandArgs& args) override {
    if (withdrawn_commands_.count(std::string(command_id))) {
      return false;
    }
    auto it = handlers_.find(std::string(command_id));
    if (it != handlers_.end() && it->second) {
      return it->second(args);
    }
    if (!catalog_) {
      return false;
    }
    tool::CommandDispatcher disp(catalog_);
    return disp.execute(command_id, args);
  }

  bool open_dialog(std::string_view dialog_id) override {
    auto it = dialogs_.find(std::string(dialog_id));
    if (it == dialogs_.end() || !it->second.factory) {
      return false;
    }
    it->second.factory(this);
    return true;
  }

  bool run_processing(std::string_view processing_id,
                      std::string_view args_json) override {
    auto it = processing_.find(std::string(processing_id));
    if (it == processing_.end() || !it->second.factory) {
      return false;
    }
    if (enqueue_) {
      return enqueue_(std::string(processing_id), std::string(args_json),
                      it->second.factory);
    }
    return it->second.factory(this, args_json);
  }

  void withdraw(std::string_view plugin_id) override {
    const std::string pid(plugin_id);
    for (auto it = handlers_.begin(); it != handlers_.end();) {
      auto owner = command_owners_.find(it->first);
      if (owner != command_owners_.end() && owner->second == pid) {
        withdrawn_commands_.insert(it->first);
        command_titles_.erase(it->first);
        command_owners_.erase(owner);
        it = handlers_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = dialogs_.begin(); it != dialogs_.end();) {
      if (it->second.plugin_id == pid) {
        it = dialogs_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = processing_.begin(); it != processing_.end();) {
      if (it->second.plugin_id == pid) {
        it = processing_.erase(it);
      } else {
        ++it;
      }
    }
    menus_.erase(std::remove_if(menus_.begin(), menus_.end(),
                                [&](const MenuRec& r) {
                                  return r.plugin_id == pid;
                                }),
                 menus_.end());
    docks_.erase(std::remove_if(docks_.begin(), docks_.end(),
                                [&](const DockRec& r) {
                                  return r.plugin_id == pid;
                                }),
                 docks_.end());
    painters_.erase(std::remove_if(painters_.begin(), painters_.end(),
                                   [&](const PainterRec& r) {
                                     return r.plugin_id == pid;
                                   }),
                    painters_.end());
    for (auto it = export_frames_.begin(); it != export_frames_.end();) {
      if (it->second.plugin_id == pid) {
        it = export_frames_.erase(it);
      } else {
        ++it;
      }
    }
    if (ui_withdraw_hook_) {
      ui_withdraw_hook_(plugin_id);
    }
  }

  bool open_dock(std::string_view dock_id) override {
    const std::string id(dock_id);
    for (const DockRec& rec : docks_) {
      if (rec.dock.id == id && rec.factory) {
        rec.factory(this);
        return true;
      }
    }
    return false;
  }

  void set_present_dataset_bridge(PresentDatasetFn fn) override {
    present_dataset_ = std::move(fn);
  }

  void set_present_surface(int surface) override {
    present_surface_ = surface != 0 ? 1 : 0;
  }

  int present_surface() const override { return present_surface_; }

  bool present_dataset(std::string_view plugin_id, std::string_view path,
                       int face) override {
    return present_dataset(plugin_id, path, face, present_surface_);
  }

  bool present_dataset(std::string_view plugin_id, std::string_view path,
                       int face, int surface) override {
    if (!present_dataset_) {
      return false;
    }
    return present_dataset_(plugin_id, path, face, surface != 0 ? 1 : 0);
  }

  GisDocument* gis_document() override { return gis_doc_; }

  void set_gis_document(GisDocument* doc) override { gis_doc_ = doc; }

  Playback* playback() override { return &playback_; }

  bool set_capability(std::string_view capability_id, void* iface) override {
    if (capability_id.empty()) {
      return false;
    }
    const std::string id(capability_id);
    if (!iface) {
      capabilities_.erase(id);
      return true;
    }
    capabilities_[id] = iface;
    return true;
  }

  void* query_capability(std::string_view capability_id) const override {
    auto it = capabilities_.find(std::string(capability_id));
    if (it == capabilities_.end()) {
      return nullptr;
    }
    return it->second;
  }

  bool contribute_export_frame(std::string_view plugin_id,
                               const ExportFrameContribution& frame) override {
    if (plugin_id.empty() || frame.id.empty()) {
      return false;
    }
    if (!(frame.max_lon > frame.min_lon && frame.max_lat > frame.min_lat)) {
      return false;
    }
    export_frames_[frame.id] = {std::string(plugin_id), frame};
    return true;
  }

  bool lookup_export_frame(std::string_view frame_id,
                           double* min_lon,
                           double* min_lat,
                           double* max_lon,
                           double* max_lat) const override {
    if (frame_id.empty() || !min_lon || !min_lat || !max_lon || !max_lat) {
      return false;
    }
    auto it = export_frames_.find(std::string(frame_id));
    if (it == export_frames_.end()) {
      return false;
    }
    *min_lon = it->second.frame.min_lon;
    *min_lat = it->second.frame.min_lat;
    *max_lon = it->second.frame.max_lon;
    *max_lat = it->second.frame.max_lat;
    return true;
  }

  void for_each_processing(
      const std::function<void(std::string_view plugin_id,
                               std::string_view processing_id,
                               std::string_view title)>& fn) const override {
    if (!fn) {
      return;
    }
    for (const auto& entry : processing_) {
      fn(entry.second.plugin_id, entry.second.proc.id, entry.second.proc.title);
    }
  }

  void for_each_dialog(
      const std::function<void(std::string_view plugin_id,
                               std::string_view dialog_id,
                               std::string_view title)>& fn) const override {
    if (!fn) {
      return;
    }
    for (const auto& entry : dialogs_) {
      fn(entry.second.plugin_id, entry.second.dialog.id,
         entry.second.dialog.title);
    }
  }

  void for_each_dock(
      const std::function<void(std::string_view plugin_id,
                               std::string_view dock_id,
                               std::string_view title)>& fn) const override {
    if (!fn) {
      return;
    }
    for (const DockRec& rec : docks_) {
      fn(rec.plugin_id, rec.dock.id, rec.dock.title);
    }
  }

  void for_each_command(
      const std::function<void(std::string_view plugin_id,
                               std::string_view command_id,
                               std::string_view title)>& fn) const override {
    if (!fn) {
      return;
    }
    for (const auto& entry : handlers_) {
      const std::string& cid = entry.first;
      if (withdrawn_commands_.contains(cid)) {
        continue;
      }
      std::string_view plugin_id;
      std::string_view title;
      const auto owner = command_owners_.find(cid);
      if (owner != command_owners_.end()) {
        plugin_id = owner->second;
      }
      const auto titled = command_titles_.find(cid);
      if (titled != command_titles_.end()) {
        title = titled->second;
      }
      fn(plugin_id, cid, title);
    }
  }

 private:
  tool::CommandCatalog* catalog_ = nullptr;
  EventBus* events_ = nullptr;
  MapContents* maps_ = nullptr;
  ProcessingEnqueue enqueue_;
  UiWithdrawHook ui_withdraw_hook_;
  PresentDatasetFn present_dataset_;
  int present_surface_ = 0;
  GisDocument* gis_doc_ = nullptr;
  Playback playback_;
  std::map<std::string, void*> capabilities_;

  std::map<std::string, tool::CommandHandler> handlers_;
  std::map<std::string, std::string> command_owners_;
  std::map<std::string, std::string> command_titles_;
  std::set<std::string> withdrawn_commands_;
  std::map<std::string, DialogRec> dialogs_;
  std::map<std::string, ProcessingRec> processing_;
  std::vector<MenuRec> menus_;
  std::vector<DockRec> docks_;
  std::vector<PainterRec> painters_;
  std::map<std::string, ExportFrameRec> export_frames_;
};

}  // namespace

PluginHost* create_plugin_host(tool::CommandCatalog* catalog,
                               EventBus* events,
                               MapContents* maps) {
  return new PluginHostImpl(catalog, events, maps);
}

}  // namespace content
