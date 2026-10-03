// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_MENU_AM_MENU_H_
#define LEGACY_UI_MAP_MENU_AM_MENU_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "legacy/core/listener/listener_manager.h"

namespace ui {
namespace detail {

// Append a separator then each aux-module popup into |menu|.
// When |insert_by_position| is true, popups are inserted by index (context menu).
void append_am_module_menus(HMENU menu,
                            base::SmtFuncItemStyle fim,
                            bool insert_by_position);

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_MAP_MENU_AM_MENU_H_
