// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_VIEW_BIND_MDI_MENU_H_
#define LEGACY_APP_VIEW_BIND_MDI_MENU_H_

#pragma once

class CSmartGisDoc;

namespace legacy_app {
namespace bind {

// Attach the Window popup to |main_menu|, store it on the document, and
// refresh the main-frame menu bar. Shared by Edit / Data / 3D views.
void attach_mdi_view_menu(HMENU main_menu, CSmartGisDoc* doc);

}  // namespace bind
}  // namespace legacy_app

#endif  // LEGACY_APP_VIEW_BIND_MDI_MENU_H_
