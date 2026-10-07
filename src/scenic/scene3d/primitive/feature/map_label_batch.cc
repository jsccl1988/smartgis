// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/feature/map_label_batch.h"

#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "vista/terrain/dem/height/dem_height_field.h"
#include "scenic/render/rhi2d/impl/gdiplus/aa/gdiplus.h"
#include "scenic/render/rhi3d/impl/d3d/ext/ext_interface.h"
#include "scenic/render/rhi3d/public/device/render_device.h"

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
#include <cstdlib>
#include <cstring>
#include <vector>
#include "base/process/switches.h"

namespace scenic {
namespace detail {
namespace {

constexpr int kLabelPx = 15;
constexpr int kHaloPx = 2;

// Screen-space collision box for 3D map labels (declutter input).
struct MapLabelBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  int priority = 0;
};

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
  // Soft warm paper ink — less chalk-white on dark DEM.
  return priority <= 1 ? RGB(248, 242, 228) : RGB(236, 240, 244);
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

  const wchar_t* faces[] = {L"Microsoft YaHei UI", L"Microsoft YaHei",
                            L"SimHei", L"SimSun"};
  Gdiplus::Font* font = nullptr;
  for (const wchar_t* face : faces) {
    auto* trial = new Gdiplus::Font(face, static_cast<Gdiplus::REAL>(px_h),
                                    Gdiplus::FontStyleRegular,
                                    Gdiplus::UnitPixel);
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
  // MeasureString can return huge/NaN bounds on bad fonts; clamp before alloc.
  constexpr int kMaxLabelDim = 1024;
  if (!(bounds.Width > 0.f) || !(bounds.Height > 0.f) ||
      bounds.Width > static_cast<Gdiplus::REAL>(kMaxLabelDim) ||
      bounds.Height > static_cast<Gdiplus::REAL>(kMaxLabelDim)) {
    delete font;
    return false;
  }
  int w = static_cast<int>(std::ceil(bounds.Width)) + pad * 2;
  int h = static_cast<int>(std::ceil(bounds.Height)) + pad * 2;
  w = (std::max)(w, 16);
  h = (std::max)(h, 16);
  if (w > kMaxLabelDim || h > kMaxLabelDim) {
    delete font;
    return false;
  }

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
  // Soft halo (not solid CAD boxes); keep alpha high enough that edges do not
  // shimmer against lit DEM under the 3D view refresh timer.
  Gdiplus::SolidBrush halo_brush(
      Gdiplus::Color(210, GetRValue(halo), GetGValue(halo), GetBValue(halo)));
  const Gdiplus::PointF center(static_cast<Gdiplus::REAL>(w) * 0.5f,
                               static_cast<Gdiplus::REAL>(h) * 0.5f);

  static const int kOff[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
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
                            RGB(14, 16, 22), &bgra, &tw, &th)) {
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

// Prefer previous-frame winners so 1px MVP jitter cannot swap city names.
void declutter_sticky(const MapLabelBox* boxes, int count, int max_keep,
                      int view_w, int view_h, const std::vector<int>& sticky,
                      std::vector<int>* keep) {
  if (!keep) {
    return;
  }
  keep->clear();
  if (!boxes || count <= 0 || max_keep <= 0) {
    return;
  }
  auto overlaps_kept = [&](int idx) -> bool {
    for (int kept : *keep) {
      const MapLabelBox& a = boxes[idx];
      const MapLabelBox& b = boxes[kept];
      if (a.left < b.right && a.right > b.left && a.top < b.bottom &&
          a.bottom > b.top) {
        return true;
      }
    }
    return false;
  };
  auto in_view = [&](const MapLabelBox& b) -> bool {
    if (view_w <= 0 || view_h <= 0) {
      return true;
    }
    return b.right > 4 && b.left < view_w - 4 && b.bottom > 4 &&
           b.top < view_h - 4;
  };
  auto try_keep = [&](int idx) {
    if (idx < 0 || idx >= count || !in_view(boxes[idx]) ||
        overlaps_kept(idx)) {
      return;
    }
    keep->push_back(idx);
  };

  std::vector<int> sticky_order = sticky;
  std::stable_sort(sticky_order.begin(), sticky_order.end(), [&](int a, int b) {
    if (a < 0 || a >= count || b < 0 || b >= count) {
      return a < b;
    }
    if (boxes[a].priority != boxes[b].priority) {
      return boxes[a].priority < boxes[b].priority;
    }
    return a < b;
  });
  for (int idx : sticky_order) {
    if (static_cast<int>(keep->size()) >= max_keep) {
      break;
    }
    try_keep(idx);
  }

  std::vector<int> order(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    order[static_cast<size_t>(i)] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    if (boxes[a].priority != boxes[b].priority) {
      return boxes[a].priority < boxes[b].priority;
    }
    return a < b;
  });
  for (int idx : order) {
    if (static_cast<int>(keep->size()) >= max_keep) {
      break;
    }
    bool already = false;
    for (int kept : *keep) {
      if (kept == idx) {
        already = true;
        break;
      }
    }
    if (!already) {
      try_keep(idx);
    }
  }
}

void draw_bitmap_fallback(LP3DRENDERDEVICE device, uint font_id, float x,
                          float y, const MapLabel& lab) {
  const Color ink = lab.priority <= 1 ? Color(1.f, 0.96f, 0.82f, 1.f)
                                         : Color(0.98f, 0.98f, 0.96f, 1.f);
  const int halo[16][2] = {
      {-2, 0},  {2, 0},  {0, -2}, {0, 2}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
      {-2, -1}, {-2, 1}, {2, -1}, {2, 1}, {-1, -2}, {1, -2}, {-1, 2}, {1, 2}};
  for (const auto& d : halo) {
    device->DrawText(font_id, x + d[0], y + d[1],
                     Color(0.05f, 0.06f, 0.08f, 0.92f), "%s",
                     lab.text.c_str());
  }
  device->DrawText(font_id, x, y, ink, "%s", lab.text.c_str());
}

}  // namespace

MapLabelBatch::MapLabelBatch() = default;

MapLabelBatch::~MapLabelBatch() { Destroy(); }

MapLabelBatch::RasterCache* MapLabelBatch::find_raster(const std::string& key) {
  for (RasterCache& entry : raster_cache_) {
    if (entry.key == key) {
      return &entry;
    }
  }
  return nullptr;
}

MapLabelBatch::RasterCache* MapLabelBatch::insert_raster(RasterCache&& entry) {
  raster_cache_.push_back(std::move(entry));
  return &raster_cache_.back();
}

long MapLabelBatch::Init(::base::Vector3& vPos, Material& matMaterial,
                         const char* szTexName) {
  return Object3d::Init(vPos, matMaterial, szTexName);
}

long MapLabelBatch::Create(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice) {
    return kErrInvalidParam;
  }
  // Geographic labels span the DEM; keep a wide AABB so frustum cull keeps us.
  m_aAbb.vcMin.set(-200.f, -50.f, -200.f);
  m_aAbb.vcMax.set(200.f, 50.f, 200.f);
  m_aAbb.vcCenter = (m_aAbb.vcMin + m_aAbb.vcMax) * 0.5f;
  // Labels draw via GDI+ textures (D3D DrawScreenBgra / GL quads). Do not
  // require GL CreateFont here — that path can heap-corrupt across the
  // scenic_impl ↔ scenic_render_gl boundary; Render falls back / skips.
  return kErrNone;
}

long MapLabelBatch::Update(LP3DRENDERDEVICE, float) { return kErrNone; }

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
                           id) == kErrNone) {
      font_id_ = id;
      font_ready_ = true;
      return true;
    }
  }
  return false;
}

long MapLabelBatch::Render(LP3DRENDERDEVICE p3DRenderDevice) {
  if (!p3DRenderDevice || labels_.empty()) {
    return kErrNone;
  }
  if (const char* skip = base::switch_cstr("rhi3d-skip-labels");
      skip && (skip[0] == '1' || skip[0] == 'y' || skip[0] == 'Y')) {
    return kErrNone;
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
  // Pad collision boxes to the drawn quad (GDI+ MeasureString + halo), not a
  // tight utf8*px estimate — short boxes let Huabei/Huadong glyphs overlap.
  const int cell = kLabelPx + 4;
  const int box_h = kLabelPx + kHaloPx * 2 + 16;
  for (const MapLabel& lab : labels_) {
    lPoint pt = {};
    if (p3DRenderDevice->Transform3DTo2D(Vector3(lab.x, lab.y, lab.z), pt) !=
        kErrNone) {
      sx.push_back(-10000);
      sy.push_back(-10000);
      MapLabelBox box;
      box.left = -10000;
      box.top = -10000;
      box.right = -9999;
      box.bottom = -9999;
      box.priority = lab.priority;
      boxes.push_back(box);
      continue;
    }
    int x = static_cast<int>(pt.x + (pt.x >= 0 ? 0.5f : -0.5f));
    int y = vh - static_cast<int>(pt.y + (pt.y >= 0 ? 0.5f : -0.5f));
    // Hold last pixel when MVP noise is ≤1px — stops declutter thrash under
    // the view refresh timer even when the camera is idle.
    const size_t li = sx.size();
    if (li < last_sx_.size() && li < last_sy_.size() &&
        std::abs(x - last_sx_[li]) <= 1 && std::abs(y - last_sy_[li]) <= 1) {
      x = last_sx_[li];
      y = last_sy_[li];
    }
    sx.push_back(x);
    sy.push_back(y);
    int w = utf8_units(lab.text) * cell + kHaloPx * 2 + 20;
    int h = box_h;
    const std::string cache_key =
        lab.text + "|" + std::to_string(lab.priority);
    if (const RasterCache* cached = find_raster(cache_key)) {
      w = cached->w + 4;
      h = cached->h + 4;
    }
    MapLabelBox box;
    box.left = x - w / 2;
    box.top = y - h / 2;
    box.right = box.left + w;
    box.bottom = box.top + h;
    box.priority = lab.priority;
    boxes.push_back(box);
  }
  std::vector<int> keep;
  // Country view: denser city set; D3D labels now draw on immediate context.
  const int budget = vw < 700 ? 28 : 40;
  declutter_sticky(boxes.data(), static_cast<int>(boxes.size()), budget, vw, vh,
                   sticky_keep_, &keep);
  sticky_keep_ = keep;
  last_sx_ = sx;
  last_sy_ = sy;

  // D3D leftover: GDI+ raster -> DrawScreenBgra (no GL context).
  if (p3DRenderDevice->GetBaseApi() == RA_D3D09) {
    for (int idx : keep) {
      const MapLabel& lab = labels_[static_cast<size_t>(idx)];
      const float x = static_cast<float>(sx[static_cast<size_t>(idx)]);
      const float y = static_cast<float>(sy[static_cast<size_t>(idx)]);
      const std::string cache_key =
          lab.text + "|" + std::to_string(lab.priority);
      RasterCache* cached = find_raster(cache_key);
      if (!cached) {
        const std::wstring wide = gis::datasource::ogr_bytes_to_wide(lab.text);
        if (wide.empty()) {
          continue;
        }
        RasterCache entry;
        entry.key = cache_key;
        if (!rasterize_label_bgra(wide, kLabelPx, kHaloPx,
                                  ink_for_priority(lab.priority),
                                  RGB(14, 16, 22), &entry.bgra, &entry.w,
                                  &entry.h)) {
          continue;
        }
        cached = insert_raster(std::move(entry));
      }
      detail::call_d3d_draw_screen_bgra(p3DRenderDevice, x, y, cached->w,
                                           cached->h, cached->bgra.data());
    }
    return kErrNone;
  }

  // Prefer GDI+ AA quads; CreateFont bitmap path is last resort only.
  const bool use_aa = gdiplus_available();
  if (!use_aa && !ensure_font(p3DRenderDevice)) {
    return kErrFailure;
  }

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

  for (int idx : keep) {
    const MapLabel& lab = labels_[static_cast<size_t>(idx)];
    const float x = static_cast<float>(sx[static_cast<size_t>(idx)]);
    const float y = static_cast<float>(sy[static_cast<size_t>(idx)]);
    if (use_aa) {
      const std::string cache_key =
          lab.text + "|" + std::to_string(lab.priority);
      RasterCache* cached = find_raster(cache_key);
      if (!cached) {
        const std::wstring wide = gis::datasource::ogr_bytes_to_wide(lab.text);
        if (wide.empty()) {
          continue;
        }
        RasterCache entry;
        entry.key = cache_key;
        if (!rasterize_label_bgra(wide, kLabelPx, kHaloPx,
                                  ink_for_priority(lab.priority),
                                  RGB(14, 16, 22), &entry.bgra, &entry.w,
                                  &entry.h)) {
          continue;
        }
        cached = insert_raster(std::move(entry));
      }
      if (cached->gl_tex == 0) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (tex == 0) {
          continue;
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cached->w, cached->h, 0, kBgra,
                     GL_UNSIGNED_BYTE, cached->bgra.data());
        cached->gl_tex = tex;
      }
      glColor4f(1.f, 1.f, 1.f, 1.f);
      draw_label_quad(x, y, cached->w, cached->h,
                      static_cast<GLuint>(cached->gl_tex));
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

  return kErrNone;
}

long MapLabelBatch::Destroy() {
  clear_raster_cache();
  labels_.clear();
  last_sx_.clear();
  last_sy_.clear();
  sticky_keep_.clear();
  font_ready_ = false;
  font_id_ = 0;
  return kErrNone;
}

void MapLabelBatch::clear_raster_cache() {
  for (RasterCache& entry : raster_cache_) {
    if (entry.gl_tex != 0 && ::wglGetCurrentContext()) {
      GLuint tex = static_cast<GLuint>(entry.gl_tex);
      glDeleteTextures(1, &tex);
      entry.gl_tex = 0;
    }
  }
  raster_cache_.clear();
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

void MapLabelBatch::clear_labels() {
  labels_.clear();
  last_sx_.clear();
  last_sy_.clear();
  sticky_keep_.clear();
  clear_raster_cache();
}

}  // namespace detail
}  // namespace scenic
