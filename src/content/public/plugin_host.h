// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_PLUGIN_HOST_H_
#define CONTENT_PUBLIC_PLUGIN_HOST_H_

#include <functional>
#include <string>
#include <string_view>

#include "content/content_export.h"
#include "content/public/event_bus.h"
#include "content/public/map_types.h"
#include "tool/command/command.h"

namespace plugin {
class ProcessingPool;
}

namespace content {

struct MenuContribution {
  std::string id;
  std::string title;
  std::string parent;  // "tools" / "file" / "view" / another menu id
};

struct DockContribution {
  std::string id;
  std::string title;
  std::string area;  // left | right | bottom | float
};

struct DialogContribution {
  std::string id;
  std::string title;
};

struct ProcessingContribution {
  std::string id;
  std::string title;
};

// Real MapContents lives in content/public/map_contents.h. Keep a forward
// declaration here so PluginHost TUs that must not include that header (and
// TUs that already include it) do not hit C2011 redefinition.
class MapContents;

class PluginHost;

using DialogFactory = std::function<void(PluginHost*)>;
using ProcessingFactory =
    std::function<bool(PluginHost*, std::string_view args_json)>;
// Header-only seam so content does not GN-dep plugin::ProcessingPool.
using ProcessingEnqueue = std::function<bool(
    std::string processing_id, std::string args_json, ProcessingFactory factory)>;
// Content must not #include ui/views. App shell installs painters via this
// callback (typically PainterRegistry::register_painter_for_plugin).
using UiPainterInstaller = std::function<void()>;
// App shell sets this so withdraw can clear Views painters without a content
// → ui/views include edge.
using UiWithdrawHook = std::function<void(std::string_view plugin_id)>;

class CONTENT_EXPORT PluginHost {
 public:
  virtual ~PluginHost() = default;

  virtual MapContents* map_contents() = 0;
  virtual EventBus* events() = 0;
  virtual tool::CommandCatalog* commands() = 0;

  virtual bool contribute_command(std::string_view plugin_id,
                                  std::string_view command_id,
                                  std::string_view title,
                                  std::string_view menu_id,
                                  tool::CommandHandler handler) = 0;
  virtual bool contribute_menu(std::string_view plugin_id,
                               const MenuContribution& menu) = 0;
  virtual bool contribute_dock(std::string_view plugin_id,
                               const DockContribution& dock,
                               DialogFactory factory) = 0;
  virtual bool contribute_dialog(std::string_view plugin_id,
                                 const DialogContribution& dialog,
                                 DialogFactory factory) = 0;
  virtual bool contribute_processing(std::string_view plugin_id,
                                     const ProcessingContribution& proc,
                                     ProcessingFactory factory) = 0;
  // Runs |install| when args are valid and records ownership for withdraw.
  virtual bool contribute_painter(std::string_view plugin_id,
                                  std::string_view role,
                                  UiPainterInstaller install) = 0;

  virtual bool execute(std::string_view command_id,
                       const tool::CommandArgs& args) = 0;
  virtual bool open_dialog(std::string_view dialog_id) = 0;
  virtual bool run_processing(std::string_view processing_id,
                              std::string_view args_json) = 0;

  virtual void withdraw(std::string_view plugin_id) = 0;
  virtual void set_ui_withdraw_hook(UiWithdrawHook hook) = 0;

  virtual plugin::ProcessingPool* processing_pool() = 0;
  virtual void set_processing_pool(plugin::ProcessingPool* pool) = 0;
  virtual void set_processing_enqueue(ProcessingEnqueue fn) = 0;

  // Commands still owned after contribute_command. Withdrawn ids are omitted.
  // |title| is the string passed to contribute_command (may be empty).
  // Appended after processing_* so cross-DLL PluginHost subclasses keep
  // withdraw / pool slots stable — never insert above.
  virtual void for_each_command(
      const std::function<void(std::string_view plugin_id,
                               std::string_view command_id,
                               std::string_view title)>& fn) const {
    (void)fn;
  }
};

CONTENT_EXPORT PluginHost* create_plugin_host(tool::CommandCatalog* catalog,
                                              EventBus* events,
                                              MapContents* maps);

}  // namespace content

#endif  // CONTENT_PUBLIC_PLUGIN_HOST_H_
