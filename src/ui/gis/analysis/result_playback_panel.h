// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_ANALYSIS_RESULT_PLAYBACK_PANEL_H_
#define UI_GIS_ANALYSIS_RESULT_PLAYBACK_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Checkbox;
class Label;
class Slider;

// Compact ResultPlayback strip: play/pause, loop, prev/next, and frame scrub.
// Hosts wire apply_analysis_frame / AnalysisPlayback fps+looping; this panel
// stays map-agnostic.
class UI_EXPORT ResultPlaybackPanel : public View {
 public:
  ResultPlaybackPanel();
  ~ResultPlaybackPanel() override;

  void set_frame_range(int frame_count);
  void set_frame_index(int index);
  int frame_index() const { return frame_index_; }
  int frame_count() const { return frame_count_; }

  void set_playing(bool on);
  bool playing() const { return playing_; }

  void set_looping(bool on);
  bool looping() const;

  void set_status_text(std::string text);

  void set_frame_change(std::function<void(int)> fn);
  void set_play_change(std::function<void(bool)> fn);
  void set_loop_change(std::function<void(bool)> fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void refresh_labels();
  void on_play_clicked();
  void on_prev_clicked();
  void on_next_clicked();
  void on_scrub(double value);

  Label* title_ = nullptr;
  Label* frame_label_ = nullptr;
  Label* status_ = nullptr;
  Button* play_ = nullptr;
  Button* prev_ = nullptr;
  Button* next_ = nullptr;
  Checkbox* loop_ = nullptr;
  Slider* scrub_ = nullptr;

  int frame_count_ = 0;
  int frame_index_ = 0;
  bool playing_ = false;
  bool scrub_notify_ = true;

  std::function<void(int)> frame_change_;
  std::function<void(bool)> play_change_;
  std::function<void(bool)> loop_change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_ANALYSIS_RESULT_PLAYBACK_PANEL_H_
