// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_ABOUT_DIALOG_H_
#define PLUGIN_WIDGETS_ABOUT_DIALOG_H_

#include <string>

#include "ui/views/view.h"

namespace plugin {

// Shared About dialog: a Label plus an OK Button.
class AboutDialog : public ui::views::View {
 public:
  explicit AboutDialog(std::string text);
};

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_ABOUT_DIALOG_H_
