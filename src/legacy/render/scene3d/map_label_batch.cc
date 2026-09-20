// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/map_label_batch.h"

#include "legacy/render/gdi/gdi_gdiplus.h"
#include "legacy/render/scene3d/dem_height_field.h"
#include "gis/datasource/gdal/ogr_text_encoding.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <objidl.h>
#include <gdiplus.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

namespace render {
namespace {

constexpr int kLabelPx = 20;
constexpr int kHaloPx = 3;

int utf8_units(const std::string& text) {
  int n = 0;
  for (unsigned char c : text) {
    if ((c & 0xc0) != 0x80) {
      ++n;
    }
  }
  return (std::max)(1, n);
}

COLORREF ink_for_priority(int priority) {
  return priority <= 1 ? RGB(255, 244, 196) : RGB(250, 250, 245);
}

// Rasterize one label with GDI+ AA + halo into premultiplied-friendly BGRA.
bool rasterize_label_bgra(const std::wstring& wide, int px_h, int halo_px,
                          COLORREF ink, COLORREF halo,
                          std::vector<unsigned char>* bgra, int* out_w,
                          int* out_h) {
  if (!bgra || !out_w || !out_h || wide.empty() || !gdiplus_ensure_started()) {
    return false;
  }
  if (px_h < 12) {
    px_h = 12;
  }
  if (halo_px < 1) {
    halo_px = 1;
  }

  const wchar_t* faces[] = {L"Microsoft YaHei UI", L"Microsoft YaHei", L"SimHei",
                            L"SimSun"};
  Gdiplus::Font* font = nullptr;
  for (const wchar_t* face : faces) {
    auto* trial = new Gdiplus::Font(face, static_cast<Gdiplus::REAL>(px_h),
                                    Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    if (trial->GetLastStatus() == Gdiplus::Ok) {
      font = trial;
      break;
    }
    delete trial;
  }
  if (!font) {
    return false;
  }
  Gdiplus::StringFormat fmt;
  fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
  fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
  fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap |
                     Gdiplus::StringFormatFlagsNoClip);

  Gdiplus::Bitmap probe(8, 8, PixelFormat32bppARGB);
  Gdiplus::Graphics measure(&probe);
  Gdiplus::RectF bounds;
  measure.MeasureString(wide.c_str(), -1, font, Gdiplus::PointF(0.f, 0.f), &fmt,
                        &bounds);
  if (measure.GetLastStatus() != Gdiplus::Ok) {
    delete font;
    return false;
  }

  const int pad = halo_px + 3;
  int w = static_cast<int>(std::ceil(bounds.Width)) + pad * 2;
  int h = static_cast<int>(std::ceil(bounds.Height)) + pad * 2;
  w = (std::max)(w, 16);
  h = (std::max)(h, 16);

  Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
  if (bmp.GetLastStatus() != Gdiplus::Ok) {
    delete font;
    return false;
  }
  Gdiplus::Graphics g(&bmp);
  g.Clear(Gdiplus::Color(0, 0, 0, 0));
  // AntiAliasGridFit (not ClearType): ClearType on transparent BG looks dirty.
  g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
  g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

  Gdiplus::SolidBrush ink_brush(
      Gdiplus::Color(255, GetRValue(ink), GetGValue(ink), GetBValue(ink)));
  Gdiplus::SolidBrush halo_brush(
      Gdiplus::Color(230, GetRValue(halo), GetGValue(halo), GetBValue(halo)));
  const Gdiplus::PointF center(static_cast<Gdiplus::REAL>(w) * 0.5f,
                               static_cast<Gdiplus::REAL>(h) * 0.5f);

  static const int kOff[8][2] = {{-1, 0}, {1, 0},  {0, -1}, {0, 1},
                                 {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  for (int s = 1; s <= halo_px; ++s) {
    for (const auto& d : kOff) {
      Gdiplus::PointF p(center.X + static_cast<Gdiplus::REAL>(d[0] * s),
                        center.Y + static_cast<Gdiplus::REAL>(d[1] * s));
      g.DrawString(wide.c_str(), -1, font, p, &fmt, &halo_brush);
    }
  }
  g.DrawString(wide.c_str(), -1, font, center, &fmt, &ink_brush);
  delete font;

  Gdiplus::BitmapData data;
  const Gdiplus::Rect rect(0, 0, w, h);
  if (bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB,
                   &data) != Gdiplus::Ok) {
    return false;
  }
  bgra->resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
  const int stride = data.Stride;
  auto* src_base = static_cast<const unsigned char*>(data.Scan0);
  // Keep GDI+ top-down row order. Screen ortho is also top-down
  // (gluOrtho2D y=0 at top) and the quad maps V=0 to the top edge, so an
  // extra GL-style bottom-up flip would invert the glyphs.
  for (int y = 0; y < h; ++y) {
    const unsigned char* row = src_base + y * stride;
    unsigned char* dst =
        bgra->data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u;
    std::memcpy(dst, row, static_cast<size_t>(w) * 4u);
  }
  bmp.UnlockBits(&data);

  *out_w = w;
  *out_h = h;
  return true;
}

void draw_label_quad(float cx, float cy, int w, int h, GLuint tex) {
  const float hw = static_cast<float>(w) * 0.5f;
  const float hh = static_cast<float>(h) * 0.5f;
  glBindTexture(GL_TEXTURE_2D, tex);
  glBegin(GL_QUADS);
  glTexCoord2f(0.f, 0.f);
  glVertex2f(cx - hw, cy - hh);
  glTexCoord2f(1.f, 0.f);
  glVertex2f(cx + hw, cy - hh);
  glTexCoord2f(1.f, 1.f);
  glVertex2f(cx + hw, cy + hh);
  glTexCoord2f(0.f, 1.f);
  glVertex2f(cx - hw, cy + hh);
  glEnd();
}

bool draw_aa_label(float x, float y, const std::string& utf8, int priority) {
  const std::wstring wide = gis::datasource::ogr_bytes_to_wide(utf8);
  if (wide.empty()) {
    return false;
  }
  std::vector<unsigned char> bgra;
  int tw = 0;
  int th = 0;
  if (!rasterize_label_bgra(wide, kLabelPx, kHaloPx, ink_for_priority(priority),
                            RGB(20, 22, 28), &bgra, &tw, &th)) {
    return false;
  }

  GLuint tex = 0;
  glGenTextures(1, &tex);
  if (tex == 0) {
    return false;
  }
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#ifndef GL_BGRA_EXT
  constexpr GLenum kBgra = 0x80E1;
#else
  constexpr GLenum kBgra = GL_BGRA_EXT;
#endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, kBgra, GL_UNSIGNED_BYTE,
               bgra.data());
  glColor4f(1.f, 1.f, 1.f, 1.f);
  draw_label_quad(x, y, tw, th, tex);
  glDeleteTextures(1, &tex);
  return true;
}

void draw_bitmap_fallback(LP3DRENDERDEVICE device, uint font_id, float x,
                          float y, const MapLabel& lab) {
  const SmtColor ink = lab.priority <= 1
                           ? SmtColor(1.f, 0.96f, 0.82f, 1.f)
                           : SmtColor(0.98f, 0.98f, 0.96f, 1.f);
  const int halo[16][2] = {{-2, 0},  {2, 0},   {0, -2}, {0, 2},  {-1, -1},
                           {1, -1},  {-1, 1},  {1, 1},  {-2, -1}, {-2, 1},
                           {2, -1},  {2, 1},   {-1, -2}, {1, -2}, {-1, 2},
                           {1, 2}};
  for (const auto& d : halo) {
    device->DrawText(font_id, x + d[0], y + d[1],
                     SmtColor(0.05f, 0.06f, 0.08f, 0.92f), "%s",
                     lab.text.c_str());
  }
  device->DrawText(font_id, x, y, ink, "%s", lab.text.c_str());
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
    // Fallback bitmap path; primary draw uses GDI+ textures when available.
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
  // Pad collision boxes: CJK glyphs are ~px wide; halo expands the footprint.
  const int cell = kLabelPx + 2;
  const int box_h = kLabelPx + kHaloPx * 2 + 8;
  for (const MapLabel& lab : labels_) {
    lPoint pt;
    p3DRenderDevice->Transform3DTo2D(Vector3(lab.x, lab.y, lab.z), pt);
    const int x = static_cast<int>(pt.x);
    const int y = vh - static_cast<int>(pt.y);
    sx.push_back(x);
    sy.push_back(y);
    const int w = utf8_units(lab.text) * cell + kHaloPx * 2 + 16;
    MapLabelBox box;
    box.left = x - w / 2;
    box.top = y - box_h / 2;
    box.right = box.left + w;
    box.bottom = box.top + box_h;
    box.priority = lab.priority;
    boxes.push_back(box);
  }
  std::vector<int> keep;
  // Country view: keep a modest set so dense plains stay readable.
  const int budget = vw < 700 ? 12 : 18;
  declutter_map_labels(boxes.data(), static_cast<int>(boxes.size()), budget, vw,
                       vh, &keep);

  GLint viewport[4] = {};
  glGetIntegerv(GL_VIEWPORT, viewport);
  const GLboolean had_depth = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean had_texture = glIsEnabled(GL_TEXTURE_2D);
  const GLboolean had_blend = glIsEnabled(GL_BLEND);
  const GLboolean had_lighting = glIsEnabled(GL_LIGHTING);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, viewport[2], viewport[3], 0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);

  const bool use_aa = gdiplus_available();
  for (int idx : keep) {
    const MapLabel& lab = labels_[static_cast<size_t>(idx)];
    const float x = static_cast<float>(sx[static_cast<size_t>(idx)]);
    const float y = static_cast<float>(sy[static_cast<size_t>(idx)]);
    if (use_aa && draw_aa_label(x, y, lab.text, lab.priority)) {
      continue;
    }
    draw_bitmap_fallback(p3DRenderDevice, font_id_, x, y, lab);
    // DrawText mutates GL state; restore for subsequent AA quads.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
  }

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  if (had_depth) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (had_lighting) {
    glEnable(GL_LIGHTING);
  } else {
    glDisable(GL_LIGHTING);
  }
  if (had_blend) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }
  if (had_texture) {
    glEnable(GL_TEXTURE_2D);
  } else {
    glDisable(GL_TEXTURE_2D);
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
