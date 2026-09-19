// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/gestures.h"

namespace tool {
namespace {

enum class StrokeMode { kPoint, kRect, kLine, kPolygon };

class StrokeInteraction final : public Interaction {
 public:
  StrokeInteraction(const char* id, StrokeMode mode, DraftCallback cb,
                    uint32_t default_flags)
      : id_(id),
        mode_(mode),
        cb_(std::move(cb)),
        default_flags_(default_flags) {}

  const char* id() const override { return id_; }

  void activate() override {
    pts_.clear();
    captured_ = false;
    has_hover_ = false;
    swallow_up_ = false;
    overlay_ = {};
  }

  void deactivate() override {
    pts_.clear();
    captured_ = false;
    has_hover_ = false;
    swallow_up_ = false;
    overlay_ = {};
  }

  void aux_draw() override { refresh_overlay(); }

  const AuxOverlay* aux_overlay() const override {
    return overlay_.kind == AuxOverlay::Kind::kNone ? nullptr : &overlay_;
  }

  bool on_input(const content::InputEvent& e) override {
    using Kind = content::InputEvent::Kind;
    switch (mode_) {
      case StrokeMode::kPoint:
        if (e.kind == Kind::kLDown) {
          finish({e.x_px, e.y_px});
          swallow_up_ = true;
          return true;
        }
        if (e.kind == Kind::kLUp) {
          if (swallow_up_) {
            swallow_up_ = false;
            return true;
          }
          finish({e.x_px, e.y_px});
          return true;
        }
        return false;
      case StrokeMode::kRect:
        if (e.kind == Kind::kLDown) {
          captured_ = true;
          pts_.clear();
          pts_.push_back({e.x_px, e.y_px});
          refresh_overlay();
          return true;
        }
        if (e.kind == Kind::kMouseMove && captured_) {
          if (pts_.size() == 1) {
            pts_.push_back({e.x_px, e.y_px});
          } else {
            pts_.back() = {e.x_px, e.y_px};
          }
          refresh_overlay();
          return true;
        }
        if (e.kind == Kind::kLUp && captured_) {
          if (pts_.empty()) {
            pts_.push_back({e.x_px, e.y_px});
          }
          if (pts_.size() == 1) {
            pts_.push_back({e.x_px, e.y_px});
          } else {
            pts_.back() = {e.x_px, e.y_px};
          }
          captured_ = false;
          emit(DraftKind::kRect);
          return true;
        }
        if (e.kind == Kind::kRDown ||
            (e.kind == Kind::kKeyDown && e.key == 0x1B)) {
          captured_ = false;
          pts_.clear();
          overlay_ = {};
          return true;
        }
        return false;
      case StrokeMode::kLine:
      case StrokeMode::kPolygon:
        if (e.kind == Kind::kLDown) {
          pts_.push_back({e.x_px, e.y_px});
          refresh_overlay();
          return true;
        }
        if (e.kind == Kind::kMouseMove) {
          hover_ = {e.x_px, e.y_px};
          has_hover_ = true;
          refresh_overlay();
          return !pts_.empty();
        }
        if (e.kind == Kind::kRDown || e.kind == Kind::kLDClick) {
          const size_t need = (mode_ == StrokeMode::kPolygon) ? 3 : 2;
          if (pts_.size() >= need) {
            emit(mode_ == StrokeMode::kPolygon ? DraftKind::kPolygon
                                                : DraftKind::kLineString);
          } else {
            pts_.clear();
            overlay_ = {};
          }
          return true;
        }
        if (e.kind == Kind::kKeyDown && e.key == 0x1B) {
          pts_.clear();
          overlay_ = {};
          has_hover_ = false;
          return true;
        }
        return false;
    }
    return false;
  }

 private:
  void finish(DraftPoint p) {
    pts_.clear();
    pts_.push_back(p);
    emit(DraftKind::kPoint);
  }

  void emit(DraftKind kind) {
    if (!cb_) {
      return;
    }
    Draft d;
    d.kind = kind;
    d.points = pts_;
    d.flags = default_flags_;
    cb_(d);
    pts_.clear();
    overlay_ = {};
    has_hover_ = false;
  }

  void refresh_overlay() {
    overlay_ = {};
    if (mode_ == StrokeMode::kRect) {
      if (!captured_ || pts_.empty()) {
        return;
      }
      overlay_.kind = AuxOverlay::Kind::kRect;
      overlay_.points.push_back({pts_[0].x_px, pts_[0].y_px});
      if (pts_.size() >= 2) {
        overlay_.points.push_back({pts_.back().x_px, pts_.back().y_px});
      }
      if (overlay_.points.size() < 2) {
        overlay_ = {};
      }
      return;
    }
    if (mode_ != StrokeMode::kLine && mode_ != StrokeMode::kPolygon) {
      return;
    }
    if (pts_.empty()) {
      return;
    }
    overlay_.kind = AuxOverlay::Kind::kPolyline;
    for (const DraftPoint& p : pts_) {
      overlay_.points.push_back({p.x_px, p.y_px});
    }
    if (has_hover_) {
      overlay_.points.push_back(hover_);
    }
    if (overlay_.points.size() < 2) {
      overlay_ = {};
    }
  }

  const char* id_;
  StrokeMode mode_;
  DraftCallback cb_;
  uint32_t default_flags_ = 0;
  std::vector<DraftPoint> pts_;
  AuxOverlay overlay_{};
  AuxPoint hover_{};
  bool captured_ = false;
  bool has_hover_ = false;
  bool swallow_up_ = false;
};

// 2D view.pan: continuous drag pan (mouse or touch); two-finger midpoint pan
// emits kRect drafts with draft_flags::kTouchPan.
class ViewPanInteraction final : public Interaction {
 public:
  ViewPanInteraction(DraftCallback cb, uint32_t default_flags)
      : cb_(std::move(cb)), default_flags_(default_flags) {}

  const char* id() const override { return "view.pan"; }

  void activate() override { reset(); }
  void deactivate() override { reset(); }

  bool on_input(const content::InputEvent& e) override {
    using Kind = content::InputEvent::Kind;
    if (content::is_multitouch(e)) {
      return on_multitouch(e);
    }
    if (multitouch_) {
      if (e.kind == Kind::kLUp && captured_) {
        emit_delta(e.x_px, e.y_px, /*touch=*/true);
        reset();
        return true;
      }
      multitouch_ = false;
    }
    if (e.kind == Kind::kLDown) {
      captured_ = true;
      pts_.clear();
      pts_.push_back({e.x_px, e.y_px});
      return true;
    }
    if (e.kind == Kind::kMouseMove && captured_) {
      emit_delta(e.x_px, e.y_px, /*touch=*/false);
      return true;
    }
    if (e.kind == Kind::kLUp && captured_) {
      emit_delta(e.x_px, e.y_px, /*touch=*/false);
      reset();
      return true;
    }
    if (e.kind == Kind::kRDown ||
        (e.kind == Kind::kKeyDown && e.key == 0x1B)) {
      reset();
      return true;
    }
    return false;
  }

 private:
  void reset() {
    pts_.clear();
    captured_ = false;
    multitouch_ = false;
  }

  bool on_multitouch(const content::InputEvent& e) {
    using Kind = content::InputEvent::Kind;
    multitouch_ = true;
    if (e.kind == Kind::kLDown ||
        (e.kind == Kind::kMouseMove && !captured_)) {
      captured_ = true;
      pts_.clear();
      pts_.push_back({e.x_px, e.y_px});
      return true;
    }
    if (e.kind == Kind::kMouseMove && captured_) {
      emit_delta(e.x_px, e.y_px, /*touch=*/true);
      return true;
    }
    if (e.kind == Kind::kLUp && captured_) {
      emit_delta(e.x_px, e.y_px, /*touch=*/true);
      reset();
      return true;
    }
    return false;
  }

  void emit_delta(int32_t x, int32_t y, bool touch) {
    if (pts_.empty()) {
      pts_.push_back({x, y});
    }
    if (pts_.size() == 1) {
      pts_.push_back({x, y});
    } else {
      pts_.back() = {x, y};
    }
    emit_keep(touch);
    if (pts_.size() >= 2) {
      pts_[0] = pts_[1];
    }
  }

  void emit_keep(bool touch) {
    if (!cb_ || pts_.size() < 2) {
      return;
    }
    Draft d;
    d.kind = DraftKind::kRect;
    d.points = pts_;
    d.flags = default_flags_;
    if (touch) {
      d.flags |= draft_flags::kTouchPan;
    }
    cb_(d);
  }

  DraftCallback cb_;
  uint32_t default_flags_ = 0;
  std::vector<DraftPoint> pts_;
  bool captured_ = false;
  bool multitouch_ = false;
};

// Pixel-space 3D camera drag / look. Camera math stays in leftover ApplyDraft.
class View3dInteraction final : public Interaction {
 public:
  View3dInteraction(const char* id, bool look_always, DraftCallback cb)
      : id_(id), look_always_(look_always), cb_(std::move(cb)) {}

  const char* id() const override { return id_; }

  void activate() override {
    pts_.clear();
    captured_ = false;
    touch_pan_ = false;
    origin_ = {};
  }

  void deactivate() override {
    pts_.clear();
    captured_ = false;
    touch_pan_ = false;
    origin_ = {};
  }

  bool on_input(const content::InputEvent& e) override {
    using Kind = content::InputEvent::Kind;
    if (content::is_multitouch(e)) {
      return on_touch_pan(e);
    }
    if (touch_pan_) {
      if (e.kind == Kind::kLUp) {
        touch_pan_ = false;
        captured_ = false;
        pts_.clear();
        return true;
      }
      touch_pan_ = false;
    }
    if (e.kind == Kind::kKeyDown) {
      emit_key(e);
      return true;
    }
    if (e.kind == Kind::kWheel) {
      emit_wheel(e);
      return true;
    }
    if (look_always_) {
      if (e.kind == Kind::kLDown) {
        captured_ = true;
        origin_ = {e.x_px, e.y_px};
        emit_look(e);
        return true;
      }
      if (e.kind == Kind::kLUp) {
        captured_ = false;
        if (e.x_px == origin_.x_px && e.y_px == origin_.y_px) {
          emit_pick(e);
        } else {
          emit_look(e);
        }
        return true;
      }
      if (e.kind == Kind::kMouseMove) {
        emit_look(e);
        return true;
      }
      return false;
    }
    if (e.kind == Kind::kLDown || e.kind == Kind::kRDown) {
      captured_ = true;
      flags_ = e.flags;
      if (e.kind == Kind::kRDown) {
        flags_ |= 0x0002;  // MK_RBUTTON
      }
      origin_ = {e.x_px, e.y_px};
      pts_.clear();
      pts_.push_back({e.x_px, e.y_px});
      // Do not emit+clear: the next move needs the press origin for orbit delta.
      return true;
    }
    if (e.kind == Kind::kMouseMove && captured_) {
      if (pts_.empty()) {
        pts_.push_back(origin_);
      }
      if (pts_.size() == 1) {
        pts_.push_back({e.x_px, e.y_px});
      } else {
        pts_.back() = {e.x_px, e.y_px};
      }
      emit_keep(DraftKind::kRect);
      if (pts_.size() >= 2) {
        pts_[0] = pts_[1];
      }
      return true;
    }
    if ((e.kind == Kind::kLUp || e.kind == Kind::kRUp) && captured_) {
      const bool click =
          e.x_px == origin_.x_px && e.y_px == origin_.y_px;
      if (pts_.empty()) {
        pts_.push_back({e.x_px, e.y_px});
      }
      if (pts_.size() == 1) {
        pts_.push_back({e.x_px, e.y_px});
      } else {
        pts_.back() = {e.x_px, e.y_px};
      }
      captured_ = false;
      if (click) {
        emit_pick(e);
      } else {
        emit(DraftKind::kRect);
      }
      return true;
    }
    if (e.kind == Kind::kRDown) {
      captured_ = false;
      pts_.clear();
      return true;
    }
    return false;
  }

 private:
  bool on_touch_pan(const content::InputEvent& e) {
    using Kind = content::InputEvent::Kind;
    touch_pan_ = true;
    // Two-finger pan overrides single-finger orbit / look capture.
    if (e.kind == Kind::kLDown ||
        (e.kind == Kind::kMouseMove && !captured_)) {
      captured_ = true;
      flags_ = draft_flags::kTouchPan;
      origin_ = {e.x_px, e.y_px};
      pts_.clear();
      pts_.push_back(origin_);
      return true;
    }
    if (e.kind == Kind::kMouseMove && captured_) {
      if (pts_.empty()) {
        pts_.push_back(origin_);
      }
      if (pts_.size() == 1) {
        pts_.push_back({e.x_px, e.y_px});
      } else {
        pts_.back() = {e.x_px, e.y_px};
      }
      flags_ = draft_flags::kTouchPan;
      emit_keep(DraftKind::kRect);
      if (pts_.size() >= 2) {
        pts_[0] = pts_[1];
      }
      return true;
    }
    if (e.kind == Kind::kLUp && captured_) {
      if (pts_.empty()) {
        pts_.push_back({e.x_px, e.y_px});
      }
      if (pts_.size() == 1) {
        pts_.push_back({e.x_px, e.y_px});
      } else {
        pts_.back() = {e.x_px, e.y_px};
      }
      flags_ = draft_flags::kTouchPan;
      emit(DraftKind::kRect);
      captured_ = false;
      touch_pan_ = false;
      return true;
    }
    return false;
  }

  void emit_look(const content::InputEvent& e) {
    pts_.clear();
    pts_.push_back({e.x_px, e.y_px});
    emit(DraftKind::kPoint);
  }

  void emit_key(const content::InputEvent& e) {
    if (!cb_) {
      return;
    }
    Draft d;
    d.kind = DraftKind::kKey;
    d.key = e.key;
    d.points.push_back({e.x_px, e.y_px});
    cb_(d);
  }

  void emit_wheel(const content::InputEvent& e) {
    if (!cb_) {
      return;
    }
    Draft d;
    d.kind = DraftKind::kWheel;
    d.wheel = e.wheel;
    d.flags = e.flags;
    d.points.push_back({e.x_px, e.y_px});
    cb_(d);
  }

  void emit_pick(const content::InputEvent& e) {
    if (!cb_) {
      return;
    }
    Draft d;
    d.kind = DraftKind::kPick;
    d.points.push_back({e.x_px, e.y_px});
    cb_(d);
  }

  void emit(DraftKind kind) {
    emit_keep(kind);
    pts_.clear();
  }

  void emit_keep(DraftKind kind) {
    if (!cb_) {
      return;
    }
    Draft d;
    d.kind = kind;
    d.points = pts_;
    d.flags = flags_;
    cb_(d);
  }

  const char* id_;
  bool look_always_ = false;
  DraftCallback cb_;
  std::vector<DraftPoint> pts_;
  DraftPoint origin_{};
  bool captured_ = false;
  bool touch_pan_ = false;
  uint32_t flags_ = 0;
};

}  // namespace

std::unique_ptr<Interaction> make_select_point(DraftCallback on_complete,
                                               uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "select.point", StrokeMode::kPoint, std::move(on_complete),
      default_flags);
}

std::unique_ptr<Interaction> make_select_rect(DraftCallback on_complete,
                                              uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "select.rect", StrokeMode::kRect, std::move(on_complete), default_flags);
}

std::unique_ptr<Interaction> make_select_circle(DraftCallback on_complete) {
  return std::make_unique<StrokeInteraction>(
      "select.circle", StrokeMode::kRect, std::move(on_complete),
      draft_flags::pack(draft_flags::kFamilySelect,
                        draft_flags::kSelectCircleCode));
}

std::unique_ptr<Interaction> make_select_polygon(DraftCallback on_complete,
                                                 uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "select.polygon", StrokeMode::kPolygon, std::move(on_complete),
      default_flags);
}

std::unique_ptr<Interaction> make_draw_point(DraftCallback on_complete,
                                             uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "draw.point", StrokeMode::kPoint, std::move(on_complete), default_flags);
}

std::unique_ptr<Interaction> make_draw_linestring(DraftCallback on_complete,
                                                  uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "draw.linestring", StrokeMode::kLine, std::move(on_complete),
      default_flags);
}

std::unique_ptr<Interaction> make_draw_polygon(DraftCallback on_complete,
                                               uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "draw.polygon", StrokeMode::kPolygon, std::move(on_complete),
      default_flags);
}

std::unique_ptr<Interaction> make_draw_rect(DraftCallback on_complete,
                                            uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "draw.rect", StrokeMode::kRect, std::move(on_complete), default_flags);
}

std::unique_ptr<Interaction> make_view_zoom_in(DraftCallback on_complete,
                                               uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "view.zoom_in", StrokeMode::kRect, std::move(on_complete),
      default_flags);
}

std::unique_ptr<Interaction> make_view_zoom_out(DraftCallback on_complete,
                                                uint32_t default_flags) {
  return std::make_unique<StrokeInteraction>(
      "view.zoom_out", StrokeMode::kPoint, std::move(on_complete),
      default_flags);
}

std::unique_ptr<Interaction> make_view_pan(DraftCallback on_complete,
                                           uint32_t default_flags) {
  return std::make_unique<ViewPanInteraction>(std::move(on_complete),
                                              default_flags);
}

std::unique_ptr<Interaction> make_view3d_trackball(DraftCallback on_complete) {
  return std::make_unique<View3dInteraction>("view3d.trackball", false,
                                             std::move(on_complete));
}

std::unique_ptr<Interaction> make_view3d_sphere(DraftCallback on_complete) {
  return std::make_unique<View3dInteraction>("view3d.sphere", false,
                                             std::move(on_complete));
}

std::unique_ptr<Interaction> make_view3d_fps(DraftCallback on_complete) {
  return std::make_unique<View3dInteraction>("view3d.fps", true,
                                             std::move(on_complete));
}

}  // namespace tool
