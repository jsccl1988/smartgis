// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TESTING_HARNESS_EVENT_GENERATOR_H_
#define UI_VIEWS_TESTING_HARNESS_EVENT_GENERATOR_H_

#include <cstdint>
#include <string_view>

namespace ui {
namespace views {

class Widget;

// Synthetic pointer/keyboard input for headless Widget tests (client coords).
class EventGenerator {
 public:
  explicit EventGenerator(Widget* widget);

  EventGenerator(const EventGenerator&) = delete;
  EventGenerator& operator=(const EventGenerator&) = delete;

  bool move_to(int x, int y);
  bool press(int button = 1);
  bool release(int button = 1);
  bool click(int x, int y, int button = 1);
  bool drag(int x0, int y0, int x1, int y1, int button = 1);
  bool key_press(std::uint32_t vk);
  bool type_char(wchar_t ch);
  bool type_utf8(std::string_view utf8);

  int last_x() const { return last_x_; }
  int last_y() const { return last_y_; }

 private:
  Widget* widget_ = nullptr;
  int last_x_ = 0;
  int last_y_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TESTING_HARNESS_EVENT_GENERATOR_H_
