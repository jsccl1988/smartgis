// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_MENU_H
#define SMT_LEGACY_CORE_MENU_H

#include "legacy/core/listener/listener_manager.h"

inline HMENU create_listener_menu(base::SmtListener* pListener,
                                  base::SmtFuncItemStyle style) {
  if (pListener == nullptr) {
    return nullptr;
  }
  HMENU hMenu = ::CreatePopupMenu();
  if (hMenu == nullptr) {
    return nullptr;
  }
  base::vSmtFuncItems items = pListener->get_func_items(style);
  for (const auto& item : items) {
    ::AppendMenuA(hMenu, MF_STRING, static_cast<UINT_PTR>(item.lMsg),
                  item.szName);
  }
  return hMenu;
}

inline void append_listener_menu(HMENU hOwnwerMenu,
                                 base::SmtListener* pListener,
                                 base::SmtFuncItemStyle style,
                                 bool bInsertSeprator = true) {
  if (hOwnwerMenu == nullptr || pListener == nullptr) {
    return;
  }
  if (bInsertSeprator) {
    ::AppendMenuA(hOwnwerMenu, MF_SEPARATOR, 0, nullptr);
  }
  base::vSmtFuncItems items = pListener->get_func_items(style);
  for (const auto& item : items) {
    ::AppendMenuA(hOwnwerMenu, MF_STRING, static_cast<UINT_PTR>(item.lMsg),
                  item.szName);
  }
}

inline bool append_popup_menu(HMENU owner, HMENU popup, const char* name) {
  if (owner == nullptr || popup == nullptr || name == nullptr) {
    return false;
  }
  return ::AppendMenuA(owner, MF_POPUP, reinterpret_cast<UINT_PTR>(popup),
                       name) != FALSE;
}

inline bool insert_popup_menu(HMENU owner, UINT position, HMENU popup,
                              const char* name, UINT extra_flags) {
  if (owner == nullptr || popup == nullptr || name == nullptr) {
    return false;
  }
  return ::InsertMenuA(owner, position, MF_POPUP | extra_flags,
                       reinterpret_cast<UINT_PTR>(popup), name) != FALSE;
}

inline bool attach_listener_popup(HMENU owner, base::SmtListener* listener,
                                  base::SmtFuncItemStyle style,
                                  const char* name, int insert_at = -1,
                                  UINT extra_flags = 0) {
  HMENU popup = create_listener_menu(listener, style);
  if (popup == nullptr) {
    return false;
  }
  if (::GetMenuItemCount(popup) <= 0) {
    ::DestroyMenu(popup);
    return false;
  }
  const char* caption =
      (name && name[0]) ? name : (listener ? listener->get_name() : nullptr);
  if (caption == nullptr || caption[0] == 0) {
    caption = "";
  }
  const bool ok =
      (insert_at < 0)
          ? append_popup_menu(owner, popup, caption)
          : insert_popup_menu(owner, static_cast<UINT>(insert_at), popup,
                              caption, extra_flags);
  if (!ok) {
    ::DestroyMenu(popup);
  }
  return ok;
}

#endif  // SMT_LEGACY_CORE_MENU_H
