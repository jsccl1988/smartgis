// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/frame/detail/collision.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_set>

namespace gis {
namespace vista {
namespace detail {
namespace {

int road_class_from_tag(const char* tag) {
  if (!tag || !tag[0]) {
    return 0;
  }
  if (std::strcmp(tag, "expressway") == 0) {
    return 3;
  }
  if (std::strcmp(tag, "highway") == 0) {
    return 2;
  }
  if (std::strcmp(tag, "road") == 0) {
    return 1;
  }
  return 0;
}

bool is_road_kind(const char* kind) {
  return road_class_from_tag(kind) != 0;
}

}  // namespace

int utf8_units(const char* text) {
  if (!text || !text[0]) {
    return 1;
  }
  int n = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
       *p; ++p) {
    if ((*p & 0xc0) != 0x80) {
      ++n;
    }
  }
  return (std::max)(1, n);
}

int label_priority(const char* name, const char* kind, const char* cls,
                   const char* adcode) {
  if (cls && std::strcmp(cls, "title") == 0) {
    return 0;
  }
  if (cls && std::strcmp(cls, "region_label") == 0) {
    return 1;
  }
  if (adcode && std::strlen(adcode) >= 6) {
    const char* digits = adcode;
    while (*digits && (*digits < '0' || *digits > '9')) {
      ++digits;
    }
    const size_t n = std::strlen(digits);
    if (n >= 6) {
      const char* tail4 = digits + n - 4;
      const char* tail2 = digits + n - 2;
      if (std::strcmp(tail4, "0000") == 0) {
        return 1;
      }
      // Provincial capital seats (…0100) stay on the country frame.
      if (std::strcmp(tail4, "0100") == 0 || std::strcmp(tail4, "0200") == 0) {
        return 1;
      }
      if (std::strcmp(tail2, "00") == 0) {
        return 2;
      }
      return 6;
    }
  }
  if (cls && std::strcmp(cls, "river_label") == 0) {
    return 3;
  }
  if (kind && (std::strcmp(kind, "river") == 0 ||
               std::strcmp(kind, "water") == 0)) {
    return 3;
  }
  if (kind && is_road_kind(kind)) {
    return 4;
  }
  if (name && *name) {
    // UTF-8 byte literals — avoid MSVC source-charset turning CJK literals
    // into the wrong encoding (then find() never matches and every city
    // falls through to priority 5 / clutter).
    const std::string n(name);
    static constexpr char kSheng[] = "\xe7\x9c\x81";              // 省
    static constexpr char kZizhiqu[] = "\xe8\x87\xaa\xe6\xb2\xbb\xe5\x8c\xba";  // 自治区
    static constexpr char kTebiexingzhengqu[] =
        "\xe7\x89\xb9\xe5\x88\xab\xe8\xa1\x8c\xe6\x94\xbf\xe5\x8c\xba";  // 特别行政区
    static constexpr char kZizhizhou[] =
        "\xe8\x87\xaa\xe6\xb2\xbb\xe5\xb7\x9e";                    // 自治州
    static constexpr char kDiqu[] = "\xe5\x9c\xb0\xe5\x8c\xba";    // 地区
    static constexpr char kMeng[] = "\xe7\x9b\x9f";                // 盟
    static constexpr char kXian[] = "\xe5\x8e\xbf";                // 县
    static constexpr char kQi[] = "\xe6\x97\x97";                  // 旗
    static constexpr char kShi[] = "\xe5\xb8\x82";                 // 市
    if (n.find(kSheng) != std::string::npos ||
        n.find(kZizhiqu) != std::string::npos ||
        n.find(kTebiexingzhengqu) != std::string::npos) {
      return 1;
    }
    if (n.find(kZizhizhou) != std::string::npos ||
        n.find(kDiqu) != std::string::npos ||
        n.find(kMeng) != std::string::npos) {
      return 2;
    }
    if (n.find(kXian) != std::string::npos || n.find(kQi) != std::string::npos) {
      return 6;
    }
    if (n.find(kShi) != std::string::npos) {
      return 2;
    }
  }
  if (kind &&
      (std::strcmp(kind, "city") == 0 || std::strcmp(kind, "point") == 0)) {
    return 2;
  }
  if (kind &&
      (std::strcmp(kind, "region") == 0 || std::strcmp(kind, "area") == 0)) {
    return 1;
  }
  return 5;
}

int lod_max_priority(float fblc) {
  // Country framing (fblc ≈ px per lon-degree). Mainland showcase sits near
  // fblc≈13 — allow prefecture / 市 seats (priority 2), not only 省.
  if (fblc < 12.f) {
    return 1;
  }
  if (fblc < 28.f) {
    return 2;
  }
  if (fblc < 60.f) {
    return 3;
  }
  return 9;
}

int label_budget(float fblc) {
  if (fblc < 12.f) {
    return 16;
  }
  if (fblc < 28.f) {
    return 28;
  }
  if (fblc < 60.f) {
    return 60;
  }
  return 180;
}

int label_px(int priority, float fblc) {
  if (priority <= 1) {
    return fblc < 12.f ? 15 : 17;
  }
  if (priority == 2) {
    return fblc < 18.f ? 14 : 16;
  }
  if (priority == 3) {
    return 13;
  }
  return 12;
}

// Exported: frame_test calls this across the gis DLL boundary.
int halo_px(int priority) {
  return priority <= 2 ? 3 : 2;
}

LabelBox label_box(int x, int y, const char* text, int px_h, int priority) {
  const int h = (std::max)(12, px_h);
  // CJK is roughly em-square; latin-only 3/5 under-estimates and lets labels
  // stack on the china country frame.
  const int units = utf8_units(text);
  const int glyph_w = (std::max)(h * 4 / 5, h * 3 / 5);
  const int w = units * glyph_w + 12;
  LabelBox box;
  box.left = x - 4;
  box.top = y - 4;
  box.right = x + w + 4;
  box.bottom = y + h + 6;
  box.priority = priority;
  return box;
}

LabelBox label_box_rotated(int x, int y, const char* text, int px_h,
                           int priority, float angle_deg) {
  const LabelBox upright = label_box(x, y, text, px_h, priority);
  const int w = upright.right - upright.left;
  const int h = upright.bottom - upright.top;
  const float rad = angle_deg * 3.14159265f / 180.f;
  const float cos_a = std::fabs(std::cos(rad));
  const float sin_a = std::fabs(std::sin(rad));
  const float rot_w =
      static_cast<float>(w) * cos_a + static_cast<float>(h) * sin_a;
  const float rot_h =
      static_cast<float>(w) * sin_a + static_cast<float>(h) * cos_a;
  const int cx = (upright.left + upright.right) / 2;
  const int cy = (upright.top + upright.bottom) / 2;
  LabelBox box;
  box.left = static_cast<int>(std::floor(cx - rot_w / 2.f)) - 2;
  box.top = static_cast<int>(std::floor(cy - rot_h / 2.f)) - 2;
  box.right = static_cast<int>(std::ceil(cx + rot_w / 2.f)) + 2;
  box.bottom = static_cast<int>(std::ceil(cy + rot_h / 2.f)) + 2;
  box.priority = priority;
  return box;
}

void expand_label_box_for_halo(LabelBox* box, float halo_width_px) {
  if (!box || halo_width_px <= 0.f) {
    return;
  }
  const int pad = static_cast<int>(std::ceil(halo_width_px)) + 1;
  box->left -= pad;
  box->top -= pad;
  box->right += pad;
  box->bottom += pad;
}

namespace {

float segment_length(int x0, int y0, int x1, int y1) {
  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  return std::hypot(dx, dy);
}

float upright_line_angle_deg(float dx, float dy) {
  if (dx == 0.f && dy == 0.f) {
    return 0.f;
  }
  float angle_deg = std::atan2(dy, dx) * 180.f / 3.14159265f;
  if (std::fabs(angle_deg) > 90.f) {
    angle_deg += angle_deg > 0.f ? -180.f : 180.f;
  }
  return angle_deg;
}

}  // namespace

bool line_label_pose(const int* xy_pairs, int n_pts, LineLabelPose* out) {
  if (!xy_pairs || n_pts < 2 || !out) {
    return false;
  }
  float total_len = 0.f;
  for (int i = 0; i < n_pts - 1; ++i) {
    total_len += segment_length(xy_pairs[i * 2], xy_pairs[i * 2 + 1],
                                xy_pairs[(i + 1) * 2], xy_pairs[(i + 1) * 2 + 1]);
  }
  if (total_len <= 0.f) {
    return false;
  }
  const float target = total_len * 0.5f;
  float walked = 0.f;
  int seg = 0;
  for (; seg < n_pts - 1; ++seg) {
    const float seg_len =
        segment_length(xy_pairs[seg * 2], xy_pairs[seg * 2 + 1],
                       xy_pairs[(seg + 1) * 2], xy_pairs[(seg + 1) * 2 + 1]);
    if (walked + seg_len >= target) {
      const float t = seg_len > 0.f ? (target - walked) / seg_len : 0.5f;
      const int x0 = xy_pairs[seg * 2];
      const int y0 = xy_pairs[seg * 2 + 1];
      const int x1 = xy_pairs[(seg + 1) * 2];
      const int y1 = xy_pairs[(seg + 1) * 2 + 1];
      out->x = static_cast<int>(std::lround(x0 + t * static_cast<float>(x1 - x0)));
      out->y = static_cast<int>(std::lround(y0 + t * static_cast<float>(y1 - y0)));
      const float dx = static_cast<float>(x1 - x0);
      const float dy = static_cast<float>(y1 - y0);
      out->angle_deg = upright_line_angle_deg(dx, dy);
      return true;
    }
    walked += seg_len;
  }
  const int last = n_pts - 1;
  const int x0 = xy_pairs[(last - 1) * 2];
  const int y0 = xy_pairs[(last - 1) * 2 + 1];
  const int x1 = xy_pairs[last * 2];
  const int y1 = xy_pairs[last * 2 + 1];
  out->x = (x0 + x1) / 2;
  out->y = (y0 + y1) / 2;
  out->angle_deg =
      upright_line_angle_deg(static_cast<float>(x1 - x0),
                             static_cast<float>(y1 - y0));
  return true;
}

bool boxes_overlap(const LabelBox& a, const LabelBox& b) {
  return a.left < b.right && a.right > b.left && a.top < b.bottom &&
         a.bottom > b.top;
}

bool LabelGrid::box_in_view(const LabelBox& box) const {
  return box.right > 4 && box.left < view_w_ - 4 && box.bottom > 4 &&
         box.top < view_h_ - 4;
}

int64_t LabelGrid::cell_key(int cell_x, int cell_y) {
  return (static_cast<int64_t>(cell_x) << 32) | static_cast<uint32_t>(cell_y);
}

void LabelGrid::insert(size_t index, const LabelBox& box, int cell_size) {
  const int cs = (std::max)(8, cell_size);
  const int x0 = box.left / cs;
  const int x1 = box.right / cs;
  const int y0 = box.top / cs;
  const int y1 = box.bottom / cs;
  for (int cy = y0; cy <= y1; ++cy) {
    for (int cx = x0; cx <= x1; ++cx) {
      grid_[cell_key(cx, cy)].push_back(index);
    }
  }
}

bool LabelGrid::overlaps_kept(const LabelBox& box, int cell_size) const {
  const int cs = (std::max)(8, cell_size);
  const int x0 = box.left / cs - 1;
  const int x1 = box.right / cs + 1;
  const int y0 = box.top / cs - 1;
  const int y1 = box.bottom / cs + 1;
  std::unordered_set<size_t> candidates;
  for (int cy = y0; cy <= y1; ++cy) {
    for (int cx = x0; cx <= x1; ++cx) {
      const auto it = grid_.find(cell_key(cx, cy));
      if (it == grid_.end()) {
        continue;
      }
      for (size_t idx : it->second) {
        candidates.insert(idx);
      }
    }
  }
  for (size_t idx : candidates) {
    if (idx < labels_.size() && boxes_overlap(box, labels_[idx])) {
      return true;
    }
  }
  return false;
}

bool LabelGrid::try_keep(const LabelBox& box) {
  if (box.priority > max_priority_) {
    return false;
  }
  if (static_cast<int>(labels_.size()) >= budget_) {
    return false;
  }
  if (!box_in_view(box)) {
    return false;
  }
  if (overlaps_kept(box, cell_size_)) {
    return false;
  }
  const size_t idx = labels_.size();
  labels_.push_back(box);
  insert(idx, box, cell_size_);
  return true;
}

}  // namespace detail
}  // namespace vista
}  // namespace gis
