// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_STATUS_BAR_H_
#define UI_VIEWS_STATUS_BAR_H_

#include <string>

#include "ui/views/view.h"

namespace ui {
namespace views {

class Label;

// Bottom strip: scale, CRS, XY, and a flexible status message.
class StatusBar : public View {
 public:
  StatusBar();

  void set_xy(double x, double y);
  void set_scale(double scale);
  void set_message(std::string text);

  void set_scale_text(std::string text);
  void set_crs_text(std::string text);
  void set_coord_text(std::string text);
  void set_status(std::string text);

  const std::string& scale_text() const;
  const std::string& crs_text() const;
  const std::string& coord_text() const;
  const std::string& status() const;
  const std::string& message() const { return status(); }

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  Label* scale_ = nullptr;
  Label* crs_ = nullptr;
  Label* coord_ = nullptr;
  Label* status_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_STATUS_BAR_H_
