// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/map_label_batch.h"

#include "legacy/render/scene3d/dem_height_field.h"
#include "gis/datasource/gdal/ogr_text_encoding.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace render {
namespace {

int utf8_units(const std::string& text) {
  int n = 0;
  for (unsigned char c : text) {
    if ((c & 0xc0) != 0x80) {
      ++n;
    }
  }
  return (std::max)(1, n);
}

}  // namespace

MapLabelBatch::MapLabelBatch() = default;

MapLabelBatch::~MapLabelBatch() { Destroy(); }

long MapLabelBatch::Init(Vector3& vPos, SmtMaterial& matMaterial,
                         const char* szTexName) {
  return Smt3DObject::Init(vPos, matMaterial, szTexName);
}

long MapLabelBatch::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice) {
    return SMT_ERR_INVALID_PARAM;
  }
  return ensure_font(p3DRenderDevice) ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

long MapLabelBatch::Update(LP3DRENDERDEVICE, float) { return SMT_ERR_NONE; }

bool MapLabelBatch::ensure_font(LP3DRENDERDEVICE device) {
  if (font_ready_ || !device) {
    return font_ready_;
  }
  const char* faces[] = {"Microsoft YaHei UI", "Microsoft YaHei", "SimHei",
                         "SimSun"};
  for (const char* face : faces) {
    uint id = 0;
    // Larger ClearType-quality glyphs; bitmap path still benefits from size.
    if (device->CreateFont(face, 0, 0, FW_SEMIBOLD, false, false, false, 18,
                           id) == SMT_ERR_NONE) {
      font_id_ = id;
      font_ready_ = true;
      return true;
    }
  }
  return false;
}

long MapLabelBatch::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice || labels_.empty()) {
    return SMT_ERR_NONE;
  }
  if (!ensure_font(p3DRenderDevice)) {
    return SMT_ERR_FAILURE;
  }
  const Viewport3D& vp = p3DRenderDevice->GetViewport();
  const int vw = static_cast<int>(vp.ulWidth);
  const int vh = static_cast<int>(vp.ulHeight);
  std::vector<MapLabelBox> boxes;
  boxes.reserve(labels_.size());
  std::vector<int> sx;
  std::vector<int> sy;
  sx.reserve(labels_.size());
  sy.reserve(labels_.size());
  for (const MapLabel& lab : labels_) {
    lPoint pt;
    p3DRenderDevice->Transform3DTo2D(Vector3(lab.x, lab.y, lab.z), pt);
    const int x = static_cast<int>(pt.x);
    const int y = vh - static_cast<int>(pt.y);
    sx.push_back(x);
    sy.push_back(y);
    const int w = utf8_units(lab.text) * 18 + 14;
    const int h = 22;
    MapLabelBox box;
    box.left = x - w / 2;
    box.top = y - h / 2;
    box.right = box.left + w;
    box.bottom = box.top + h;
    box.priority = lab.priority;
    boxes.push_back(box);
  }
  std::vector<int> keep;
  // Country view: fewer labels, less overlap clutter.
  const int budget = vw < 700 ? 14 : 22;
  declutter_map_labels(boxes.data(), static_cast<int>(boxes.size()), budget, vw,
                       vh, &keep);
  // Thicker dark outline (2px ring) reads cleaner than a 1px halo on GL bitmaps.
  const int halo[16][2] = {{-2, 0},  {2, 0},  {0, -2}, {0, 2},  {-1, -1},
                           {1, -1},  {-1, 1}, {1, 1},  {-2, -1}, {-2, 1},
                           {2, -1},  {2, 1},  {-1, -2}, {1, -2}, {-1, 2},
                           {1, 2}};
  for (int idx : keep) {
    const MapLabel& lab = labels_[static_cast<size_t>(idx)];
    const float x = static_cast<float>(sx[static_cast<size_t>(idx)]);
    const float y = static_cast<float>(sy[static_cast<size_t>(idx)]);
    const SmtColor ink = lab.priority <= 1
                             ? SmtColor(1.f, 0.96f, 0.82f, 1.f)
                             : SmtColor(0.98f, 0.98f, 0.96f, 1.f);
    for (const auto& d : halo) {
      p3DRenderDevice->DrawText(font_id_, x + d[0], y + d[1],
                                SmtColor(0.05f, 0.06f, 0.08f, 0.92f), "%s",
                                lab.text.c_str());
    }
    p3DRenderDevice->DrawText(font_id_, x, y, ink, "%s", lab.text.c_str());
  }
  return SMT_ERR_NONE;
}

long MapLabelBatch::Destroy() {
  labels_.clear();
  font_ready_ = false;
  font_id_ = 0;
  return SMT_ERR_NONE;
}

void MapLabelBatch::add_label(const MapLabel& label) {
  if (label.text.empty()) {
    return;
  }
  labels_.push_back(label);
  labels_.back().text = gis::datasource::ogr_bytes_to_utf8(label.text);
  m_aAbb.merge(label.x, label.y, label.z);
  m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin) / 2.f;
}

void MapLabelBatch::clear_labels() { labels_.clear(); }

}  // namespace render
