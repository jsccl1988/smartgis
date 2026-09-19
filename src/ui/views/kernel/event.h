// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_EVENT_H_
#define UI_VIEWS_KERNEL_EVENT_H_

#include <cstdint>

namespace ui {
namespace views {

// Pointer input in widget client coordinates.
struct MouseEvent {
  enum class Type {
    kMove,
    kDown,
    kUp,
    kDblClick,
    kWheel,
  };

  Type type = Type::kMove;
  int x = 0;
  int y = 0;
  int flags = 0;
  int button = 0;  // 1 = left, 2 = right, 3 = middle
  int wheel_delta = 0;
};

// Keyboard input (virtual-key).
struct KeyEvent {
  enum class Type {
    kDown,
    kUp,
  };

  Type type = Type::kDown;
  std::uint32_t vk = 0;
  int flags = 0;
};

// Translated character (WM_CHAR). Used by Textfield.
struct CharEvent {
  wchar_t ch = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_EVENT_H_
