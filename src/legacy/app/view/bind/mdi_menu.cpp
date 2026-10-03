// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"

#include "legacy/app/view/bind/mdi_menu.h"

#include "legacy/app/doc/document.h"
#include "legacy/app/shell/frame/win_app.h"

namespace legacy_app {
namespace bind {

void attach_mdi_view_menu(HMENU main_menu, CSmartGisDoc* doc) {
  theApp.append_mdi_window_menu(main_menu);
  if (doc) {
    doc->m_hCurMainMenu = main_menu;
  }
  if (CFrameWnd* frame = static_cast<CFrameWnd*>(AfxGetMainWnd())) {
    frame->OnUpdateFrameMenu(NULL);
  }
  if (CWnd* main = AfxGetMainWnd()) {
    main->DrawMenuBar();
  }
}

}  // namespace bind
}  // namespace legacy_app
