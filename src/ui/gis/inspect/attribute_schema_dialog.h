// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_INSPECT_ATTRIBUTE_SCHEMA_DIALOG_H_
#define UI_GIS_INSPECT_ATTRIBUTE_SCHEMA_DIALOG_H_

#include "ui/ui_export.h"
#include <string>
#include <vector>

#include <windows.h>

namespace ui {
namespace views {

// One attribute-schema row: field name plus a type label (Integer/Double/...).
struct AttributeField {
  std::string name;
  std::string type;
};

// Modal field-schema editor that replaces leftover AttStructEditDlg.
// Hosts map AttributeField rows to product attribute types after accept.
class UI_EXPORT AttributeSchemaDialog {
 public:
  static bool run(HWND owner, std::vector<AttributeField>* fields);
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_INSPECT_ATTRIBUTE_SCHEMA_DIALOG_H_
