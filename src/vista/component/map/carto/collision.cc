// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/carto/collision.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

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

namespace {

bool next_utf8_cp(const unsigned char** pp, uint32_t* cp) {
  const unsigned char* p = *pp;
  if (!p || !*p) {
    return false;
  }
  if (p[0] < 0x80) {
    *cp = p[0];
    *pp = p + 1;
    return true;
  }
  if ((p[0] & 0xe0) == 0xc0 && p[1]) {
    *cp = (static_cast<uint32_t>(p[0] & 0x1f) << 6) |
          static_cast<uint32_t>(p[1] & 0x3f);
    *pp = p + 2;
    return true;
  }
  if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
    *cp = (static_cast<uint32_t>(p[0] & 0x0f) << 12) |
          (static_cast<uint32_t>(p[1] & 0x3f) << 6) |
          static_cast<uint32_t>(p[2] & 0x3f);
    *pp = p + 3;
    return true;
  }
  if ((p[0] & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
    *cp = (static_cast<uint32_t>(p[0] & 0x07) << 18) |
          (static_cast<uint32_t>(p[1] & 0x3f) << 12) |
          (static_cast<uint32_t>(p[2] & 0x3f) << 6) |
          static_cast<uint32_t>(p[3] & 0x3f);
    *pp = p + 4;
    return true;
  }
  *cp = p[0];
  *pp = p + 1;
  return true;
}

bool is_cjk_or_fullwidth(uint32_t cp) {
  return (cp >= 0x1100 && cp <= 0x11ff) || (cp >= 0x2e80 && cp <= 0x9fff) ||
         (cp >= 0xa000 && cp <= 0xa4ff) || (cp >= 0xac00 && cp <= 0xd7af) ||
         (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0xfe30 && cp <= 0xfe4f) ||
         (cp >= 0xff01 && cp <= 0xff60) || (cp >= 0xffe0 && cp <= 0xffe6) ||
         (cp >= 0x20000 && cp <= 0x2fa1f);
}

}  // namespace

float estimate_run_width_px(const char* text, float px_h) {
  const float h = (std::max)(1.f, px_h);
  if (!text || !text[0]) {
    return h * 0.55f;
  }
  float width = 0.f;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  uint32_t cp = 0;
  while (next_utf8_cp(&p, &cp)) {
    // Full-width / CJK ≈ em square; Latin / digits ≈ half-em (bitmap stub).
    width += is_cjk_or_fullwidth(cp) ? h * 0.95f : h * 0.55f;
  }
  return (std::max)(h * 0.55f, width);
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
  // Latin-only hydro / line names (Oka, Ravi, Son, Betwa…) steal overview
  // budget and collide with Chinese city labels — demote unless CJK present.
  const bool name_cjk = name && *name && [&]() {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(name);
    uint32_t cp = 0;
    while (next_utf8_cp(&p, &cp)) {
      if (is_cjk_or_fullwidth(cp)) {
        return true;
      }
    }
    return false;
  }();
  if (cls && std::strcmp(cls, "river_label") == 0) {
    return name_cjk ? 3 : 5;
  }
  if (kind && (std::strcmp(kind, "river") == 0 ||
               std::strcmp(kind, "water") == 0 ||
               std::strcmp(kind, "line") == 0)) {
    return name_cjk ? 3 : 5;
  }
  if (kind && is_road_kind(kind)) {
    return 4;
  }
  if (name && *name) {
    // UTF-8 byte literals — avoid MSVC source-charset turning CJK literals
    // into the wrong encoding (then find() never matches and every city
    // falls through to priority 5 / clutter).
    const std::string n(name);
    // Tier-1 municipalities — keep on china overview even under label budget.
    static constexpr const char* kTier1[] = {
        "\xe5\x8c\x97\xe4\xba\xac",  // 北京
        "\xe4\xb8\x8a\xe6\xb5\xb7",  // 上海
        "\xe5\xb9\xbf\xe5\xb7\x9e",  // 广州
        "\xe6\xb7\xb1\xe5\x9c\xb3",  // 深圳
        "\xe9\x87\x8d\xe5\xba\x86",  // 重庆
        "\xe5\xa4\xa9\xe6\xb4\xa5",  // 天津
    };
    for (const char* t1 : kTier1) {
      if (n.find(t1) != std::string::npos) {
        return 0;
      }
    }
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

int label_draw_budget(double scale) {
  // Caps from the retired whole-string label pass.
  if (scale < 22.0) {
    return 24;
  }
  if (scale < 48.0) {
    return 40;
  }
  if (scale < 96.0) {
    return 80;
  }
  return 120;
}

int point_label_nudges(int text_h, PointNudge* out, int max_out) {
  if (!out || max_out <= 0) {
    return 0;
  }
  const int h = text_h > 0 ? text_h : 18;
  // Prefer north / east / west before south — china overview coastal cities
  // (深圳/台北/南宁) were nudged into the ocean by early +Y slots.
  const int dxs[8] = {0, 0, 14, -14, 18, -18, 0, 0};
  const int dys[8] = {0, -(h + 4), 0, 0, -(h / 2), -(h / 2), h + 4, h + 2};
  const int n = (std::min)(max_out, 8);
  for (int i = 0; i < n; ++i) {
    out[i].dx = dxs[i];
    out[i].dy = dys[i];
  }
  return n;
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
  // Mixed-script estimate: CJK ≈ em, Latin ≈ 0.55em (see estimate_run_width_px).
  const int w =
      static_cast<int>(
          std::ceil(estimate_run_width_px(text, static_cast<float>(h)))) +
      12;
  LabelBox box;
  box.left = x - 4;
  box.top = y - 4;
  box.right = x + w + 4;
  box.bottom = y + h + 6;
  box.priority = priority;
  return box;
}

LabelBox rotate_label_box(const LabelBox& upright, float angle_deg) {
  if (angle_deg == 0.f) {
    return upright;
  }
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
  box.priority = upright.priority;
  return box;
}

LabelBox label_box_rotated(int x, int y, const char* text, int px_h,
                           int priority, float angle_deg) {
  return rotate_label_box(label_box(x, y, text, px_h, priority), angle_deg);
}

void expand_label_box_for_halo(LabelBox* box, float halo_width_px) {
  if (!box || halo_width_px <= 0.f) {
    return;
  }
  // +2 beyond stroke so neighboring city labels do not kiss through the halo.
  const int pad = static_cast<int>(std::ceil(halo_width_px)) + 3;
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

float line_path_length_px(const int* xy_pairs, int n_pts) {
  if (!xy_pairs || n_pts < 2) {
    return 0.f;
  }
  float total_len = 0.f;
  for (int i = 0; i < n_pts - 1; ++i) {
    total_len += segment_length(xy_pairs[i * 2], xy_pairs[i * 2 + 1],
                                xy_pairs[(i + 1) * 2], xy_pairs[(i + 1) * 2 + 1]);
  }
  return total_len;
}

bool line_label_pose_at_with_length(const int* xy_pairs, int n_pts,
                                    float total_len_px, float fraction,
                                    LineLabelPose* out) {
  if (!xy_pairs || n_pts < 2 || !out || total_len_px <= 0.f) {
    return false;
  }
  const float clamped = (std::min)(1.f, (std::max)(0.f, fraction));
  const float target = total_len_px * clamped;
  float walked = 0.f;
  for (int seg = 0; seg < n_pts - 1; ++seg) {
    const float seg_len =
        segment_length(xy_pairs[seg * 2], xy_pairs[seg * 2 + 1],
                       xy_pairs[(seg + 1) * 2], xy_pairs[(seg + 1) * 2 + 1]);
    if (walked + seg_len >= target || seg == n_pts - 2) {
      const float t = seg_len > 0.f
                          ? (std::min)(1.f, (std::max)(0.f, (target - walked) / seg_len))
                          : 0.5f;
      const int x0 = xy_pairs[seg * 2];
      const int y0 = xy_pairs[seg * 2 + 1];
      const int x1 = xy_pairs[(seg + 1) * 2];
      const int y1 = xy_pairs[(seg + 1) * 2 + 1];
      out->x = static_cast<int>(std::lround(x0 + t * static_cast<float>(x1 - x0)));
      out->y = static_cast<int>(std::lround(y0 + t * static_cast<float>(y1 - y0)));
      out->angle_deg = upright_line_angle_deg(static_cast<float>(x1 - x0),
                                              static_cast<float>(y1 - y0));
      return true;
    }
    walked += seg_len;
  }
  return false;
}

bool line_label_pose_at(const int* xy_pairs, int n_pts, float fraction,
                        LineLabelPose* out) {
  return line_label_pose_at_with_length(
      xy_pairs, n_pts, line_path_length_px(xy_pairs, n_pts), fraction, out);
}

bool line_label_pose(const int* xy_pairs, int n_pts, LineLabelPose* out) {
  return line_label_pose_at(xy_pairs, n_pts, 0.5f, out);
}

int line_label_slot_fractions(int max_out, float* out_fractions) {
  // Midpoint first, then alternate quarters — MapLibre-ish multi-slot retry.
  static constexpr float kSlots[] = {0.50f, 0.35f, 0.65f, 0.25f, 0.75f, 0.15f,
                                     0.85f};
  if (!out_fractions || max_out <= 0) {
    return 0;
  }
  const int n = (std::min)(max_out, static_cast<int>(sizeof(kSlots) / sizeof(kSlots[0])));
  for (int i = 0; i < n; ++i) {
    out_fractions[i] = kSlots[i];
  }
  return n;
}

bool line_fits_label(float path_len_px, float text_width_px) {
  if (path_len_px <= 0.f || text_width_px <= 0.f) {
    return false;
  }
  // Need a little slack past the glyph run so endpoints stay clear.
  return path_len_px >= text_width_px * 1.05f;
}

bool boxes_overlap(const LabelBox& a, const LabelBox& b) {
  return a.left < b.right && a.right > b.left && a.top < b.bottom &&
         a.bottom > b.top;
}

bool LabelGrid::box_in_view(const LabelBox& box) const {
  // Fully inside the framebuffer. Overlap-with-margin let edge cities
  // (齐齐哈尔, clipped "田") emit glyphs that the HWND then crops.
  constexpr int kPad = 2;
  return box.left >= kPad && box.top >= kPad &&
         box.right <= view_w_ - kPad && box.bottom <= view_h_ - kPad;
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
  if (!box_in_view(box)) {
    return false;
  }
  // Prefer lower priority number (tier-1). Displace overlapping lower-rank
  // labels so 北京 is not skipped after 天津 filled the same screen cell.
  std::vector<LabelBox> kept;
  kept.reserve(labels_.size() + 1);
  for (const LabelBox& old : labels_) {
    if (!boxes_overlap(old, box)) {
      kept.push_back(old);
      continue;
    }
    if (box.priority >= old.priority) {
      return false;
    }
    // Drop |old| (lower rank).
  }
  if (static_cast<int>(kept.size()) >= budget_) {
    return false;
  }
  kept.push_back(box);
  labels_.clear();
  grid_.clear();
  for (const LabelBox& b : kept) {
    const size_t idx = labels_.size();
    labels_.push_back(b);
    insert(idx, b, cell_size_);
  }
  return true;
}

}  // namespace detail
}  // namespace vista
