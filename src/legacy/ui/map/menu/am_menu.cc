// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/menu/am_menu.h"

#include "legacy/core/util/menu.h"
#include "legacy/plugin/runtime/auxmodule/module_manager.h"

namespace ui {
namespace detail {

void append_am_module_menus(HMENU menu,
                            base::SmtFuncItemStyle fim,
                            bool insert_by_position) {
  if (!menu) {
    return;
  }
  ::AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
  plugin::SmtAModuleManager* mgr =
      plugin::SmtAModuleManager::get_singleton_ptr();
  if (!mgr) {
    return;
  }
  for (int i = 0; i < mgr->get_a_module_count(); ++i) {
    plugin::SmtAuxModule* module = mgr->get_a_module(i);
    if (!module) {
      continue;
    }
    if (insert_by_position) {
      attach_listener_popup(menu, module, fim, module->get_name(), i + 3,
                            MF_BYPOSITION);
    } else {
      attach_listener_popup(menu, module, fim, module->get_name());
    }
  }
}

}  // namespace detail
}  // namespace ui
