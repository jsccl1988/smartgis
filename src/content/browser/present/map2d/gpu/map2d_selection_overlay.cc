// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/gpu/map2d_selection_overlay.h"

#include <cmath>
#include <cstring>

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"

namespace content {
namespace detail {
namespace {

constexpr uint32_t kSelectRgba = 0xFFC87828u;  // RGB(200,120,40)
constexpr uint32_t kFlashRgba = 0xFFFFDC28u;   // RGB(255,220,40)
constexpr float kStrokePx = 3.f;
constexpr float kPointRadiusPx = 6.f;

void mix_bytes(uint64_t* h, const void* p, size_t n) {
  const auto* b = static_cast<const unsigned char*>(p);
  for (size_t i = 0; i < n; ++i) {
    *h ^= b[i];
    *h *= 1099511628211ull;
  }
}

void mix_u64(uint64_t* h, uint64_t v) {
  *h ^= v;
  *h *= 1099511628211ull;
}

double world_per_pixel(const ViewFrame* frame, uint32_t width_px,
                       uint32_t height_px) {
  if (!frame || width_px == 0 || height_px == 0) {
    return 1.0;
  }
  const double scale = frame->scale();
  if (scale <= 0.0) {
    return 1.0;
  }
  return 1.0 / scale;
}

void append_circle(vista::MapIR* ir, double cx, double cy, double radius,
                   uint32_t rgba) {
  vista::DrawItem item;
  item.kind = vista::DrawKind::kCircle;
  item.rgba = rgba;
  item.opacity = 1.f;
  constexpr int kSeg = 24;
  item.vertices.resize(static_cast<size_t>(kSeg) + 1);
  item.vertices[0] = vista::Vertex{static_cast<float>(cx), static_cast<float>(cy),
                                   0, 0, 0};
  constexpr float kPi = 3.14159265f;
  for (int i = 0; i < kSeg; ++i) {
    const float a =
        (2.f * kPi * static_cast<float>(i)) / static_cast<float>(kSeg);
    item.vertices[static_cast<size_t>(i) + 1] = vista::Vertex{
        static_cast<float>(cx + radius * std::cos(a)),
        static_cast<float>(cy + radius * std::sin(a)), 0, 0, 0};
  }
  item.indices.reserve(static_cast<size_t>(kSeg) * 3);
  for (int i = 0; i < kSeg; ++i) {
    const int next = (i + 1) % kSeg;
    item.indices.push_back(0);
    item.indices.push_back(static_cast<uint32_t>(i + 1));
    item.indices.push_back(static_cast<uint32_t>(next + 1));
  }
  ir->items.push_back(std::move(item));
}

void append_polyline_ribbon(vista::MapIR* ir,
                            const std::vector<GisScene::Vertex>& pts,
                            double half_w, uint32_t rgba) {
  if (!ir || pts.size() < 2 || half_w <= 0.0) {
    return;
  }
  vista::DrawItem item;
  item.kind = vista::DrawKind::kLine;
  item.rgba = rgba;
  item.opacity = 1.f;
  item.vertices.reserve((pts.size() - 1) * 4);
  item.indices.reserve((pts.size() - 1) * 6);
  for (size_t i = 0; i + 1 < pts.size(); ++i) {
    const double x0 = pts[i].x;
    const double y0 = pts[i].y;
    const double x1 = pts[i + 1].x;
    const double y1 = pts[i + 1].y;
    double dx = x1 - x0;
    double dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-12) {
      continue;
    }
    dx /= len;
    dy /= len;
    const double nx = -dy * half_w;
    const double ny = dx * half_w;
    const uint32_t base = static_cast<uint32_t>(item.vertices.size());
    item.vertices.push_back(
        vista::Vertex{static_cast<float>(x0 + nx), static_cast<float>(y0 + ny),
                      0, 0, 0});
    item.vertices.push_back(
        vista::Vertex{static_cast<float>(x0 - nx), static_cast<float>(y0 - ny),
                      0, 0, 0});
    item.vertices.push_back(
        vista::Vertex{static_cast<float>(x1 - nx), static_cast<float>(y1 - ny),
                      0, 0, 0});
    item.vertices.push_back(
        vista::Vertex{static_cast<float>(x1 + nx), static_cast<float>(y1 + ny),
                      0, 0, 0});
    item.indices.push_back(base);
    item.indices.push_back(base + 1);
    item.indices.push_back(base + 2);
    item.indices.push_back(base);
    item.indices.push_back(base + 2);
    item.indices.push_back(base + 3);
  }
  if (!item.indices.empty()) {
    ir->items.push_back(std::move(item));
  }
}

void append_feature_stroke(vista::MapIR* ir, const GisScene::Feature& f,
                           double half_w, double point_r, uint32_t rgba) {
  if (f.points.empty()) {
    return;
  }
  if (f.kind == GisScene::GeomKind::kPoint ||
      f.kind == GisScene::GeomKind::kText) {
    append_circle(ir, f.points.front().x, f.points.front().y, point_r, rgba);
    return;
  }
  if (f.kind == GisScene::GeomKind::kPolygon && f.points.size() >= 3) {
    if (f.points.front().x != f.points.back().x ||
        f.points.front().y != f.points.back().y) {
      std::vector<GisScene::Vertex> closed = f.points;
      closed.push_back(f.points.front());
      append_polyline_ribbon(ir, closed, half_w, rgba);
      return;
    }
  }
  append_polyline_ribbon(ir, f.points, half_w, rgba);
}

}  // namespace

uint64_t map2d_selection_signature(const GisScene* scene, bool flash_pulse) {
  uint64_t h = 14695981039346656037ull;
  mix_u64(&h, flash_pulse ? 1ull : 0ull);
  if (!scene) {
    return h;
  }
  for (const GisScene::Layer& layer : scene->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const GisScene::Feature& f : layer.features) {
      if (!f.selected) {
        continue;
      }
      mix_bytes(&h, f.id.bytes, f.id.len);
      mix_u64(&h, static_cast<uint64_t>(f.kind));
      mix_u64(&h, static_cast<uint64_t>(f.points.size()));
    }
  }
  if (const GisScene::Feature* flash = scene->selected_feature()) {
    mix_bytes(&h, flash->id.bytes, flash->id.len);
    mix_u64(&h, 0xF1A5ull);
  }
  return h;
}

void append_map2d_selection_overlay(const GisScene* scene,
                                    const ViewFrame* frame,
                                    uint32_t width_px,
                                    uint32_t height_px,
                                    bool flash_pulse,
                                    vista::MapIR* ir) {
  if (!scene || !ir || width_px == 0 || height_px == 0) {
    return;
  }
  const double wupp = world_per_pixel(frame, width_px, height_px);
  const double half_w = 0.5 * static_cast<double>(kStrokePx) * wupp;
  const double point_r = static_cast<double>(kPointRadiusPx) * wupp;

  for (const GisScene::Layer& layer : scene->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const GisScene::Feature& f : layer.features) {
      if (!f.selected) {
        continue;
      }
      append_feature_stroke(ir, f, half_w, point_r, kSelectRgba);
    }
  }
  if (flash_pulse) {
    if (const GisScene::Feature* f = scene->selected_feature()) {
      append_feature_stroke(ir, *f, half_w * 1.25, point_r * 1.15, kFlashRgba);
    }
  }
}

}  // namespace detail
}  // namespace content
