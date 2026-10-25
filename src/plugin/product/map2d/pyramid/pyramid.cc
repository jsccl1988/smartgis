// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/pyramid/pyramid.h"

#include "base/core/log.h"
#include "gis/tile/protocol/lonlat_xyz.h"
#include "gis/tile/provider/xyz_source.h"
#include "content/public/gis_contents.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"
#include "ui/views/map/viewport/horizon_paint.h"

#include <rapidjson/document.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <wincodec.h>

namespace plugin {
namespace {

struct Session {
  std::mutex mu;
  content::PluginHost* host = nullptr;
  gis::tile::XyzSource source;
  MapViewBox extent;
  bool has_extent = false;
  int stamp = 0;
  HWND overlay = nullptr;
  FurnitureOptions furniture;
};

Session& session() {
  static Session value;
  return value;
}

struct PixelPoint {
  int x = 0;
  int y = 0;
};

PixelPoint project(const MapViewBox& view, double lon, double lat) {
  PixelPoint pt;
  const double span_x = view.east - view.west;
  const double span_y = view.north - view.south;
  if (!(span_x > 0.0) || !(span_y > 0.0) || view.width_px < 1 ||
      view.height_px < 1) {
    return pt;
  }
  const double u = (lon - view.west) / span_x;
  const double v = (view.north - lat) / span_y;
  pt.x = static_cast<int>(std::lround(u * static_cast<double>(view.width_px)));
  pt.y = static_cast<int>(std::lround(v * static_cast<double>(view.height_px)));
  return pt;
}

bool live_extent(MapViewBox* view) {
  if (!view) {
    return false;
  }
  content::PluginHost* host = nullptr;
  {
    std::lock_guard<std::mutex> lock(session().mu);
    host = session().host;
    if (session().has_extent) {
      view->west = session().extent.west;
      view->south = session().extent.south;
      view->east = session().extent.east;
      view->north = session().extent.north;
    }
  }
  if (!host) {
    return view->east > view->west && view->north > view->south;
  }
  content::GisContents* gis = host->gis_contents();
  if (!gis) {
    return view->east > view->west && view->north > view->south;
  }
  for (std::uint32_t id = 1; id < 8; ++id) {
    const content::Extent2 box = gis->Extent(id);
    if (box.xmax > box.xmin && box.ymax > box.ymin) {
      view->west = box.xmin;
      view->south = box.ymin;
      view->east = box.xmax;
      view->north = box.ymax;
      return true;
    }
  }
  return view->east > view->west && view->north > view->south;
}

bool decode_bgra(const std::string& bytes, std::vector<std::uint8_t>* bgra, int* w,
                 int* h) {
  if (!bgra || !w || !h || bytes.size() < 8) {
    return false;
  }
  const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninit = co == S_OK;
  IWICImagingFactory* factory = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) {
    if (uninit) {
      CoUninitialize();
    }
    return false;
  }
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
  if (!mem) {
    factory->Release();
    if (uninit) {
      CoUninitialize();
    }
    return false;
  }
  void* locked = GlobalLock(mem);
  if (locked) {
    std::memcpy(locked, bytes.data(), bytes.size());
    GlobalUnlock(mem);
  }
  IStream* stream = nullptr;
  hr = CreateStreamOnHGlobal(mem, TRUE, &stream);
  if (FAILED(hr) || !stream) {
    GlobalFree(mem);
    factory->Release();
    if (uninit) {
      CoUninitialize();
    }
    return false;
  }
  IWICBitmapDecoder* decoder = nullptr;
  hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad,
                                        &decoder);
  stream->Release();
  if (FAILED(hr) || !decoder) {
    factory->Release();
    if (uninit) {
      CoUninitialize();
    }
    return false;
  }
  IWICBitmapFrameDecode* frame = nullptr;
  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr) || !frame) {
    decoder->Release();
    factory->Release();
    if (uninit) {
      CoUninitialize();
    }
    return false;
  }
  IWICFormatConverter* converter = nullptr;
  hr = factory->CreateFormatConverter(&converter);
  if (FAILED(hr) || !converter) {
    frame->Release();
    decoder->Release();
    factory->Release();
    if (uninit) {
      CoUninitialize();
    }
    return false;
  }
  hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                            nullptr, 0.0, WICBitmapPaletteTypeCustom);
  UINT width = 0;
  UINT height = 0;
  converter->GetSize(&width, &height);
  bool ok = SUCCEEDED(hr) && width > 0 && height > 0 && width < 8192 && height < 8192;
  if (ok) {
    bgra->resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    hr = converter->CopyPixels(nullptr, width * 4,
                              static_cast<UINT>(bgra->size()), bgra->data());
    ok = SUCCEEDED(hr);
    *w = static_cast<int>(width);
    *h = static_cast<int>(height);
  }
  converter->Release();
  frame->Release();
  decoder->Release();
  factory->Release();
  if (uninit) {
    CoUninitialize();
  }
  return ok;
}

void blit_bgra(HDC hdc, int x, int y, int w, int h, const std::uint8_t* pixels,
               int src_w, int src_h) {
  if (!hdc || !pixels || w < 1 || h < 1 || src_w < 1 || src_h < 1) {
    return;
  }
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = src_w;
  info.bmiHeader.biHeight = -src_h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  StretchDIBits(hdc, x, y, w, h, 0, 0, src_w, src_h, pixels, &info, DIB_RGB_COLORS,
                SRCCOPY);
}

void paint_cells(ui::gfx::Canvas* canvas, const MapViewBox& view) {
  if (!canvas) {
    return;
  }
  gis::tile::LonLatBox box;
  box.west = view.west;
  box.south = view.south;
  box.east = view.east;
  box.north = view.north;
  const std::vector<gis::tile::TileIndex> tiles =
      gis::tile::tiles_for_extent(box, -1, view.width_px, 32);
  for (const gis::tile::TileIndex& tile : tiles) {
    const gis::tile::LonLatBox geo =
        gis::tile::tile_lonlat_bbox(tile.z, tile.x, tile.y);
    const PixelPoint nw = project(view, geo.west, geo.north);
    const PixelPoint se = project(view, geo.east, geo.south);
    const int x = std::min(nw.x, se.x);
    const int y = std::min(nw.y, se.y);
    const int w = std::max(1, std::abs(se.x - nw.x));
    const int h = std::max(1, std::abs(se.y - nw.y));
    const ui::gfx::Color stroke = ui::gfx::color_rgb(30, 70, 110);
    std::string bytes;
    bool hit = false;
    {
      std::lock_guard<std::mutex> lock(session().mu);
      hit = session().source.try_cached(tile.z, tile.x, tile.y, &bytes);
    }
    if (hit) {
      std::vector<std::uint8_t> bgra;
      int iw = 0;
      int ih = 0;
      if (decode_bgra(bytes, &bgra, &iw, &ih)) {
        blit_bgra(canvas->hdc(), x, y, w, h, bgra.data(), iw, ih);
      }
    } else {
      const int shade = 180 + ((tile.x + tile.y) & 3) * 12;
      canvas->fill_rect(x, y, w, h, ui::gfx::color_rgb(shade, shade - 20, 160));
    }
    canvas->stroke_rect(x, y, w, h, stroke, 1);
  }
}

void paint_into(HDC hdc, int width_px, int height_px, bool color_key) {
  if (!hdc || width_px < 32 || height_px < 32) {
    return;
  }
  MapViewBox view;
  view.width_px = width_px;
  view.height_px = height_px;
  if (!live_extent(&view)) {
    view.west = -180;
    view.south = -gis::tile::k_max_mercator_lat;
    view.east = 180;
    view.north = gis::tile::k_max_mercator_lat;
  }
  bool pyramid_open = false;
  FurnitureOptions furniture;
  content::PluginHost* host = nullptr;
  {
    std::lock_guard<std::mutex> lock(session().mu);
    pyramid_open = !session().source.url_template().empty();
    furniture = session().furniture;
    host = session().host;
  }
  // Map2dSink is one object on PluginHost. Prefer it once the shell has
  // installed furniture bridges so a horizon hook in another module still
  // sees the checkbox flags. Session keeps the pyramid-side copy.
  if (Map2dSink* sink = map2d_sink(host)) {
    if (sink->furniture_bridges_installed()) {
      furniture.scale_bar = sink->furniture_scale_bar();
      furniture.north_arrow = sink->furniture_north_arrow();
      furniture.border = sink->furniture_border();
    }
  }
  ui::gfx::Canvas canvas(hdc, width_px, height_px);
  if (color_key) {
    canvas.fill_rect(0, 0, width_px, height_px, ui::gfx::color_rgb(255, 0, 255));
  }
  if (pyramid_open) {
    paint_cells(&canvas, view);
  }
  paint_furniture(&canvas, layout_furniture(view), furniture);
}

const wchar_t k_chrome_class[] = L"SmartGisMap2dChrome";

LRESULT CALLBACK chrome_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCHITTEST) {
    return HTTRANSPARENT;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void ensure_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = chrome_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = k_chrome_class;
  RegisterClassExW(&wc);
  done = true;
}

void sync_overlay(HWND map, int width_px, int height_px) {
  if (!map || width_px < 64 || height_px < 64) {
    return;
  }
  RECT wr = {};
  if (!GetWindowRect(map, &wr)) {
    return;
  }
  static int shown_stamp = -1;
  static int shown_w = 0;
  static int shown_h = 0;
  static int shown_x = 0;
  static int shown_y = 0;
  int stamp = 0;
  {
    std::lock_guard<std::mutex> lock(session().mu);
    stamp = session().stamp;
  }
  if (session().overlay && IsWindow(session().overlay) && stamp == shown_stamp &&
      shown_w == width_px && shown_h == height_px && shown_x == wr.left &&
      shown_y == wr.top) {
    return;
  }
  shown_stamp = stamp;
  shown_w = width_px;
  shown_h = height_px;
  shown_x = wr.left;
  shown_y = wr.top;
  ensure_class();
  Session& state = session();
  if (!state.overlay || !IsWindow(state.overlay)) {
    state.overlay = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        k_chrome_class, L"", WS_POPUP, wr.left, wr.top, width_px, height_px, map,
        nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!state.overlay) {
      return;
    }
  }
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width_px;
  info.bmiHeader.biHeight = -height_px;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(mem, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib) {
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  paint_into(mem, width_px, height_px, true);
  POINT dst{wr.left, wr.top};
  SIZE size{width_px, height_px};
  POINT src{0, 0};
  UpdateLayeredWindow(state.overlay, screen, &dst, &size, mem, &src, RGB(255, 0, 255),
                      nullptr, ULW_COLORKEY);
  ShowWindow(state.overlay, SW_SHOWNOACTIVATE);
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}

void prefetch(const MapViewBox& view) {
  gis::tile::LonLatBox box;
  box.west = view.west;
  box.south = view.south;
  box.east = view.east;
  box.north = view.north;
  const std::vector<gis::tile::TileIndex> tiles =
      gis::tile::tiles_for_extent(box, -1, view.width_px > 0 ? view.width_px : 1024,
                                     8);
  std::string tmpl;
  {
    std::lock_guard<std::mutex> lock(session().mu);
    tmpl = session().source.url_template();
  }
  const bool file_tmpl = tmpl.find("file:") == 0 || tmpl.find(":\\") != std::string::npos ||
                         (tmpl.size() > 2 && tmpl[1] == ':');
  if (!file_tmpl && !gis::tile::network_available()) {
    LOGGING(LOG_INFO, "map2d pyramid prefetch skipped: offline");
    return;
  }
  for (const gis::tile::TileIndex& tile : tiles) {
    std::string body;
    std::string error;
    if (!session().source.get(tile.z, tile.x, tile.y, &body, &error)) {
      LOGGING(LOG_INFO, "map2d pyramid tile %d/%d/%d miss: %s", tile.z, tile.x, tile.y,
              error.empty() ? "miss" : error.c_str());
      break;
    }
  }
}

}  // namespace

void set_map_furniture(const FurnitureOptions& options) {
  std::lock_guard<std::mutex> lock(session().mu);
  session().furniture = options;
}

FurnitureOptions map_furniture() {
  std::lock_guard<std::mutex> lock(session().mu);
  return session().furniture;
}

void bind_map_horizon(content::PluginHost* host) {
  session().host = host;
  ui::views::set_map_horizon_paint(&paint_map_horizon);
}

void note_view_extent(double west, double south, double east, double north) {
  std::lock_guard<std::mutex> lock(session().mu);
  session().extent.west = west;
  session().extent.south = south;
  session().extent.east = east;
  session().extent.north = north;
  session().has_extent = east > west && north > south;
  ++session().stamp;
}

bool open_pyramid(std::string_view args) {
  std::string tmpl(args);
  double west = 0;
  double south = 0;
  double east = 0;
  double north = 0;
  bool has_box = false;
  if (!args.empty() && args.front() == '{') {
    rapidjson::Document doc;
    doc.Parse(args.data(), args.size());
    if (!doc.HasParseError() && doc.IsObject()) {
      if (doc.HasMember("template") && doc["template"].IsString()) {
        tmpl = doc["template"].GetString();
      }
      if (doc.HasMember("west") && doc.HasMember("south") && doc.HasMember("east") &&
          doc.HasMember("north") && doc["west"].IsNumber() && doc["south"].IsNumber() &&
          doc["east"].IsNumber() && doc["north"].IsNumber()) {
        west = doc["west"].GetDouble();
        south = doc["south"].GetDouble();
        east = doc["east"].GetDouble();
        north = doc["north"].GetDouble();
        has_box = true;
      }
    }
  }
  if (tmpl.find("{z}") == std::string::npos || tmpl.find("{x}") == std::string::npos ||
      tmpl.find("{y}") == std::string::npos) {
    LOGGING(LOG_ERROR, "map2d.open_pyramid: template needs {z}/{x}/{y}");
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(session().mu);
    session().source.set_url_template(tmpl);
    if (has_box) {
      session().extent.west = west;
      session().extent.south = south;
      session().extent.east = east;
      session().extent.north = north;
      session().has_extent = true;
    }
  }
  MapViewBox view;
  view.width_px = 1280;
  view.height_px = 720;
  if (!live_extent(&view)) {
    view.west = 73;
    view.south = 18;
    view.east = 135;
    view.north = 54;
    note_view_extent(view.west, view.south, view.east, view.north);
  }
  prefetch(view);
  LOGGING(LOG_INFO, "map2d.open_pyramid template set");
  std::lock_guard<std::mutex> lock(session().mu);
  ++session().stamp;
  return true;
}

void paint_map_horizon(HWND hwnd, HDC hdc, int width_px, int height_px) {
  if (hdc && width_px > 0 && height_px > 0) {
    paint_into(hdc, width_px, height_px, false);
  }
  if (hwnd) {
    sync_overlay(hwnd, width_px, height_px);
  }
}

}  // namespace plugin
