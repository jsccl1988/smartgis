// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MESSAGE_BOX_H_
#define UI_VIEWS_MESSAGE_BOX_H_

#include <string>

namespace ui {
namespace views {

enum class MessageBoxKind { kInfo, kError };

void show_message_box(MessageBoxKind kind, const std::string& text);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MESSAGE_BOX_H_
