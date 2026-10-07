// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gfx/display_list/display_list.h"

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

void DisplayList::append_from_clipped(const DisplayList& other, int l, int t,
                                      int r, int b) {
  if (other.cmds_.empty() || r <= l || b <= t) {
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
