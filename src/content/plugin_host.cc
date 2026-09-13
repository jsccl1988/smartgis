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

class PluginHostImpl final : public PluginHost {
 public:
  PluginHostImpl(tool::CommandCatalog* catalog,
                 EventBus* events,
                 MapContents* maps)
      : catalog_(catalog), events_(events), maps_(maps) {}

  MapContents* map_contents() override { return maps_; }
  EventBus* events() override { return events_; }
  tool::CommandCatalog* commands() override { return catalog_; }
  plugin::ProcessingPool* processing_pool() override { return pool_; }

  void set_processing_pool(plugin::ProcessingPool* pool) override {
    pool_ = pool;
  }

  void set_processing_enqueue(ProcessingEnqueue fn) override {
    enqueue_ = std::move(fn);
  }

  bool contribute_command(std::string_view plugin_id,
                          std::string_view command_id,
                          std::string_view title,
                          std::string_view menu_id,
                          tool::CommandHandler handler) override {
    (void)title;
    (void)menu_id;
    if (plugin_id.empty() || command_id.empty() || !handler) {
      return false;
    }
    const std::string cid(command_id);
    if (handlers_.contains(cid) && !withdrawn_commands_.contains(cid)) {
      return false;
    }
    if (catalog_ && !catalog_->contains(cid)) {
      if (!catalog_->add(cid, handler)) {
        return false;
      }
    }
    withdrawn_commands_.erase(cid);
    handlers_[cid] = std::move(handler);
    command_owners_[cid] = std::string(plugin_id);
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

  bool execute(std::string_view command_id,
               const tool::CommandArgs& args) override {
    if (withdrawn_commands_.count(std::string(command_id))) {
      return false;
    }
    auto it = handlers_.find(std::string(command_id));
    if (it == handlers_.end()) {
      if (!catalog_) {
        return false;
      }
      tool::CommandDispatcher disp(catalog_);
      return disp.execute(command_id, args);
    }
    return it->second(args);
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
  }

 private:
  tool::CommandCatalog* catalog_ = nullptr;
  EventBus* events_ = nullptr;
  MapContents* maps_ = nullptr;
  plugin::ProcessingPool* pool_ = nullptr;
  ProcessingEnqueue enqueue_;

  std::map<std::string, tool::CommandHandler> handlers_;
  std::map<std::string, std::string> command_owners_;
  std::set<std::string> withdrawn_commands_;
  std::map<std::string, DialogRec> dialogs_;
  std::map<std::string, ProcessingRec> processing_;
  std::vector<MenuRec> menus_;
  std::vector<DockRec> docks_;
};

}  // namespace

PluginHost* create_plugin_host(tool::CommandCatalog* catalog,
                               EventBus* events,
                               MapContents* maps) {
  return new PluginHostImpl(catalog, events, maps);
}

}  // namespace content
