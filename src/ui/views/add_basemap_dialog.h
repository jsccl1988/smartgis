// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_ADD_BASEMAP_DIALOG_H_
#define UI_VIEWS_ADD_BASEMAP_DIALOG_H_

#include <string>

#include <windows.h>

namespace ui {
namespace views {

// Modal form: online basemap URL (XYZ or WMTS template) + optional name.
// Does not depend on sdb::tile; host calls make_xyz_map_layer / WMTS APIs.
class AddBasemapDialog {
 public:
  struct Result {
    std::string name;
    // "xyz" or "wmts"
    std::string kind;
    std::string url;
  };

  static bool run(HWND owner, Result* out);
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_ADD_BASEMAP_DIALOG_H_
