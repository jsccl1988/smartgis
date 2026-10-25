// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/display_list/display_list.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace ui {
namespace gfx {
namespace {

thread_local DisplayList* g_recorder = nullptr;

}  // namespace

void display_list_begin(DisplayList* list) {
  g_recorder = list;
}

void display_list_end() {
  g_recorder = nullptr;
}

DisplayList* display_list_recorder() {
  return g_recorder;
}

void DisplayList::append_from(const DisplayList& other) {
  if (other.cmds_.empty()) {
    return;
  }
  const std::uint32_t text_base = static_cast<std::uint32_t>(text_.size());
  text_.insert(text_.end(), other.text_.begin(), other.text_.end());
  cmds_.reserve(cmds_.size() + other.cmds_.size());
  for (Cmd cmd : other.cmds_) {
    if (cmd.op == Op::kText) {
      cmd.text += text_base;
    }
    cmds_.push_back(cmd);
  }
}

namespace {

constexpr std::size_t kSpatialIndexMinOps = 64;

}  // namespace

void DisplayList::ensure_spatial_index() const {
  if (!spatial_dirty_ && spatial_for_size_ == cmds_.size()) {
    return;
  }
  spatial_cells_.clear();
  if (cmds_.empty()) {
    spatial_cols_ = spatial_rows_ = 0;
    spatial_for_size_ = 0;
    spatial_dirty_ = false;
    return;
  }
  int max_x = 0;
  int max_y = 0;
  for (const Cmd& cmd : cmds_) {
    if (cmd.op == Op::kSave || cmd.op == Op::kRestore || cmd.op == Op::kClip) {
      continue;
    }
    int x1 = cmd.x + cmd.w;
    int y1 = cmd.y + cmd.h;
    if (cmd.op == Op::kLine) {
      x1 = greater(cmd.x, cmd.x1) + cmd.stroke;
      y1 = greater(cmd.y, cmd.y1) + cmd.stroke;
    }
    max_x = greater(max_x, x1);
    max_y = greater(max_y, y1);
  }
  const int cell = spatial_cell_ > 0 ? spatial_cell_ : 48;
  spatial_cols_ = greater(1, (max_x + cell) / cell);
  spatial_rows_ = greater(1, (max_y + cell) / cell);
  spatial_cells_.assign(
      static_cast<std::size_t>(spatial_cols_ * spatial_rows_), {});
  for (std::uint32_t i = 0; i < cmds_.size(); ++i) {
    const Cmd& cmd = cmds_[i];
    if (cmd.op == Op::kSave || cmd.op == Op::kRestore || cmd.op == Op::kClip) {
      continue;
    }
    int x0 = cmd.x;
    int y0 = cmd.y;
    int x1 = cmd.x + cmd.w;
    int y1 = cmd.y + cmd.h;
    if (cmd.op == Op::kLine) {
      x0 = lesser(cmd.x, cmd.x1);
      y0 = lesser(cmd.y, cmd.y1);
      x1 = greater(cmd.x, cmd.x1) + cmd.stroke;
      y1 = greater(cmd.y, cmd.y1) + cmd.stroke;
    }
    const int cx0 = greater(0, x0 / cell);
    const int cy0 = greater(0, y0 / cell);
    const int cx1 = lesser(spatial_cols_ - 1, x1 / cell);
    const int cy1 = lesser(spatial_rows_ - 1, y1 / cell);
    for (int cy = cy0; cy <= cy1; ++cy) {
      for (int cx = cx0; cx <= cx1; ++cx) {
        spatial_cells_[static_cast<std::size_t>(cy * spatial_cols_ + cx)]
            .push_back(i);
      }
    }
  }
  spatial_for_size_ = cmds_.size();
  spatial_dirty_ = false;
}

void DisplayList::append_from_clipped_indexed(const DisplayList& other, int l,
                                              int t, int r, int b) {
  other.ensure_spatial_index();
  const int cell = other.spatial_cell_ > 0 ? other.spatial_cell_ : 48;
  const int cx0 = greater(0, l / cell);
  const int cy0 = greater(0, t / cell);
  const int cx1 =
      other.spatial_cols_ > 0
          ? lesser(other.spatial_cols_ - 1, (r - 1) / cell)
          : 0;
  const int cy1 =
      other.spatial_rows_ > 0
          ? lesser(other.spatial_rows_ - 1, (b - 1) / cell)
          : 0;
  std::unordered_set<std::uint32_t> candidates;
  for (int cy = cy0; cy <= cy1; ++cy) {
    for (int cx = cx0; cx <= cx1; ++cx) {
      const auto& bucket = other.spatial_cells_[static_cast<std::size_t>(
          cy * other.spatial_cols_ + cx)];
      for (std::uint32_t idx : bucket) {
        candidates.insert(idx);
      }
    }
  }
  std::vector<std::uint32_t> text_map(other.text_.size(),
                                      static_cast<std::uint32_t>(-1));
  cmds_.reserve(cmds_.size() + other.cmds_.size());
  for (std::uint32_t i = 0; i < other.cmds_.size(); ++i) {
    const Cmd& src = other.cmds_[i];
    if (src.op == Op::kSave || src.op == Op::kRestore || src.op == Op::kClip) {
      Cmd cmd = src;
      cmds_.push_back(cmd);
      continue;
    }
    if (!candidates.count(i)) {
      continue;
    }
    Cmd cmd = src;
    if (!hits(cmd, l, t, r, b)) {
      continue;
    }
    if (cmd.op == Op::kText) {
      if (cmd.text >= other.text_.size()) {
        continue;
      }
      if (text_map[cmd.text] == static_cast<std::uint32_t>(-1)) {
        text_map[cmd.text] = static_cast<std::uint32_t>(text_.size());
        text_.push_back(other.text_[cmd.text]);
      }
      cmd.text = text_map[cmd.text];
    }
    cmds_.push_back(cmd);
  }
}

void DisplayList::append_from_clipped(const DisplayList& other, int l, int t,
                                      int r, int b) {
  if (other.cmds_.empty() || r <= l || b <= t) {
    return;
  }
  if (other.cmds_.size() >= kSpatialIndexMinOps) {
    append_from_clipped_indexed(other, l, t, r, b);
    return;
  }
  // Remap only texts that survive the clip filter (avoid copying dead strings).
  std::vector<std::uint32_t> text_map(other.text_.size(),
                                      static_cast<std::uint32_t>(-1));
  cmds_.reserve(cmds_.size() + other.cmds_.size());
  for (Cmd cmd : other.cmds_) {
    if (cmd.op == Op::kSave || cmd.op == Op::kRestore || cmd.op == Op::kClip) {
      cmds_.push_back(cmd);
      continue;
    }
    if (!hits(cmd, l, t, r, b)) {
      continue;
    }
    if (cmd.op == Op::kText) {
      if (cmd.text >= other.text_.size()) {
        continue;
      }
      if (text_map[cmd.text] == static_cast<std::uint32_t>(-1)) {
        text_map[cmd.text] = static_cast<std::uint32_t>(text_.size());
        text_.push_back(other.text_[cmd.text]);
      }
      cmd.text = text_map[cmd.text];
    }
    cmds_.push_back(cmd);
  }
}

DisplayList DisplayList::clone() const {
  DisplayList out;
  out.cmds_ = cmds_;
  out.text_ = text_;
  return out;
}

}  // namespace gfx
}  // namespace ui
