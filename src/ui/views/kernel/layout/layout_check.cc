// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout/layout_check.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"

namespace ui {
namespace views {
namespace {

std::string format_rect(const Rect& r) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "(%d,%d,%d,%d)", r.x, r.y, r.width, r.height);
  return buf;
}

void walk(const View* v,
          const View* parent,
          const Rect* parent_bounds,
          std::vector<std::string>* out,
          int* count) {
  if (!v || !v->is_visible()) {
    return;
  }
  const Rect& b = v->bounds();
  if (!rect_non_negative(b)) {
    if (out) {
      out->push_back("negative-bounds@" + format_rect(b));
    }
    ++(*count);
  }
  // Views that opt in via allows_child_overflow() (ScrollView content,
  // Combobox dropdown overlay) are not layout dislocations.
  const bool exempt_overflow = parent && parent->allows_child_overflow();
  if (!exempt_overflow && parent_bounds && b.width > 0 && b.height > 0 &&
      !rect_contains_rect(*parent_bounds, b)) {
    if (out) {
      out->push_back("child-outside-parent@" + format_rect(*parent_bounds) +
                     ">" + format_rect(b));
    }
    ++(*count);
  }
  // Leaves with a preferred size but zero laid-out size are usually a bug
  // (except intentionally collapsed splitter panes).
  if (v->child_count() == 0 && v->preferred_size().width > 0 &&
      v->preferred_size().height > 0 && (b.width <= 0 || b.height <= 0)) {
    if (out) {
      out->push_back("zero-size-leaf@" + format_rect(b));
    }
    ++(*count);
  }
  for (size_t i = 0; i < v->child_count(); ++i) {
    walk(v->child_at(i), v, &b, out, count);
  }
}

void walk_sibling_overlaps(const View* v,
                           std::vector<std::string>* out,
                           int* count) {
  if (!v || !v->is_visible()) {
    return;
  }
  if (!v->allows_child_overflow()) {
    std::vector<const View*> kids;
    kids.reserve(v->child_count());
    for (size_t i = 0; i < v->child_count(); ++i) {
      const View* c = v->child_at(i);
      if (c && c->is_visible() && c->bounds().width > 0 &&
          c->bounds().height > 0) {
        kids.push_back(c);
      }
    }
    for (size_t i = 0; i < kids.size(); ++i) {
      for (size_t j = i + 1; j < kids.size(); ++j) {
        if (!rects_overlap_positive(kids[i]->bounds(), kids[j]->bounds())) {
          continue;
        }
        if (out) {
          out->push_back("sibling-overlap@" + format_rect(v->bounds()) + ">" +
                         format_rect(kids[i]->bounds()) + "x" +
                         format_rect(kids[j]->bounds()));
        }
        ++(*count);
      }
    }
  }
  for (size_t i = 0; i < v->child_count(); ++i) {
    walk_sibling_overlaps(v->child_at(i), out, count);
  }
}

std::uint32_t fnv1a_hash(const void* data, size_t len) {
  auto* p = static_cast<const unsigned char*>(data);
  std::uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

}  // namespace

bool rect_non_negative(const Rect& r) {
  return r.width >= 0 && r.height >= 0;
}

bool rect_contains_rect(const Rect& outer, const Rect& inner) {
  if (inner.width <= 0 || inner.height <= 0) {
    return true;
  }
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
}

bool rect_approximately_centered(const Rect& inner,
                                 const Rect& outer,
                                 int tol_px) {
  if (tol_px < 0) {
    tol_px = 0;
  }
  const int ix = inner.x + inner.width / 2;
  const int iy = inner.y + inner.height / 2;
  const int ox = outer.x + outer.width / 2;
  const int oy = outer.y + outer.height / 2;
  const int dx = ix > ox ? ix - ox : ox - ix;
  const int dy = iy > oy ? iy - oy : oy - iy;
  return dx <= tol_px && dy <= tol_px;
}

bool rects_overlap_positive(const Rect& a, const Rect& b) {
  if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) {
    return false;
  }
  const int left = (std::max)(a.x, b.x);
  const int top = (std::max)(a.y, b.y);
  const int right = (std::min)(a.right(), b.right());
  const int bottom = (std::min)(a.bottom(), b.bottom());
  return right > left && bottom > top;
}

int collect_layout_violations(const View* root, std::vector<std::string>* out) {
  int count = 0;
  walk(root, nullptr, nullptr, out, &count);
  return count;
}

int collect_sibling_overlaps(const View* root, std::vector<std::string>* out) {
  int count = 0;
  walk_sibling_overlaps(root, out, &count);
  return count;
}

bool menu_item_metrics_ok(int item_width_px,
                          int item_height_px,
                          float scale) {
  if (scale <= 0.f) {
    scale = 1.f;
  }
  const int min_h = dip_to_px(22, scale);
  const int min_w = dip_to_px(24, scale);
  return item_width_px >= min_w && item_height_px >= min_h;
}

bool write_layout_issues_file(const std::filesystem::path& path,
                              const std::vector<std::string>& issues) {
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  if (issues.empty()) {
    out << "# clean\n";
    return static_cast<bool>(out);
  }
  for (const std::string& line : issues) {
    out << line << '\n';
  }
  return static_cast<bool>(out);
}

std::uint32_t paint_fingerprint(View* root, int width, int height) {
  if (!root || width <= 0 || height <= 0) {
    return 0;
  }
  root->set_bounds({0, 0, width, height});
  root->layout();

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = width;
  bi.bmiHeader.biHeight = -height;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP dib =
      CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return 0;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  {
    ui::gfx::Canvas canvas(mem, width, height);
    root->paint(&canvas);
  }
  const size_t nbytes =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  const std::uint32_t hash = fnv1a_hash(bits, nbytes);
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return hash;
}

}  // namespace views
}  // namespace ui
