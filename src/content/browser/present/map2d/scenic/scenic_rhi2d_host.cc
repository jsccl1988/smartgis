// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/view_frame.h"
#include "content/browser/present/host/scenic_scene_bind.h"
#include "content/browser/present/map2d/scenic/scenic_rhi2d_host.h"
#include "scenic/engine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "base/core/log.h"
#include "base/process/switches.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/view_frame.h"
#include "content/public/map_bootstrap.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "scenic/render/err.h"
#include "scenic/render/rhi2d/public/device/renderer.h"
#include "scenic/render/rhi2d/public/device/viewport.h"

namespace content {
namespace detail {
namespace {

const char* api_for_port(const char* port) {
  if (!port || port[0] == '\0') {
    return "GdiRenderDevice";
  }
  if (_stricmp(port, "gdiplus") == 0 || _stricmp(port, "gdi+") == 0) {
    return "GdiPlusRenderDevice";
  }
  if (_stricmp(port, "skia") == 0) {
    return "SkiaRenderDevice";
  }
  return "GdiRenderDevice";
}

std::string exe_dir() {
  char buf[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return ".";
  }
  return std::filesystem::path(buf).parent_path().string();
}

}  // namespace

bool map2d_bmp_is_ocean_clear(const char* path) {
  if (!path || path[0] == '\0') {
    return false;
  }
  HBITMAP bmp =
      static_cast<HBITMAP>(LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0,
                                      LR_LOADFROMFILE | LR_CREATEDIBSECTION));
  if (!bmp) {
    return false;
  }
  BITMAP bm = {};
  GetObject(bmp, sizeof(bm), &bm);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bm.bmWidth;
  bmi.bmiHeader.biHeight = -bm.bmHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  const int pixels = bm.bmWidth * bm.bmHeight;
  if (pixels <= 0) {
    DeleteObject(bmp);
    return false;
  }
  std::vector<std::uint32_t> bits(static_cast<size_t>(pixels));
  HDC hdc = GetDC(nullptr);
  const int got = GetDIBits(hdc, bmp, 0, static_cast<UINT>(bm.bmHeight),
                            bits.data(), &bmi, DIB_RGB_COLORS);
  ReleaseDC(nullptr, hdc);
  DeleteObject(bmp);
  if (got <= 0) {
    return false;
  }
  int ocean = 0;
  int samples = 0;
  for (int i = 0; i < pixels; i += 16) {
    const unsigned b = bits[static_cast<size_t>(i)] & 0xffu;
    const unsigned g = (bits[static_cast<size_t>(i)] >> 8) & 0xffu;
    const unsigned r = (bits[static_cast<size_t>(i)] >> 16) & 0xffu;
    ++samples;
    if (std::abs(static_cast<int>(r) - 170) <= 8 &&
        std::abs(static_cast<int>(g) - 211) <= 8 &&
        std::abs(static_cast<int>(b) - 223) <= 8) {
      ++ocean;
    }
  }
  return samples > 0 &&
         (static_cast<float>(ocean) / static_cast<float>(samples)) > 0.92f;
}

struct ScenicRhi2dHost::DeviceState {
  scenic::detail::Renderer2d renderer{GetModuleHandleA(nullptr)};
};

ScenicRhi2dHost::ScenicRhi2dHost() = default;

ScenicRhi2dHost::~ScenicRhi2dHost() { shutdown(); }

void ScenicRhi2dHost::shutdown() {
  shutdown_draw_engine();
  device_.reset();
  hwnd_ = nullptr;
  width_px_ = 0;
  height_px_ = 0;
  last_ok_ = false;
  map_.DeleteAll();
  if (dataset_) {
    GDALClose(dataset_);
    dataset_ = nullptr;
  }
}

bool ScenicRhi2dHost::is_ready() const {
  return device_ && device_->renderer.GetDevice() != nullptr && hwnd_ != nullptr;
}

bool ScenicRhi2dHost::ensure_map() {
  if (map_.GetLayerCount() > 0) {
    return true;
  }
  GDALAllRegister();
  std::string path;
  const std::string dir = exe_dir();
  if (!try_resolve_existing_sample_map({dir, ".", dir + "/.."}, &path)) {
    LOGGING(LOG_ERROR, "scenic rhi2d: no china sample map beside exe");
    return false;
  }
  dataset_ = static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr,
      nullptr));
  if (!dataset_ || dataset_->GetLayerCount() < 1) {
    LOGGING(LOG_ERROR, "scenic rhi2d: GDALOpenEx failed path=%s", path.c_str());
    if (dataset_) {
      GDALClose(dataset_);
      dataset_ = nullptr;
    }
    return false;
  }
  for (int i = 0; i < dataset_->GetLayerCount(); ++i) {
    OGRLayer* lyr = dataset_->GetLayer(i);
    if (lyr) {
      map_.AddLayer(lyr);
    }
  }
  if (map_.GetLayerCount() < 1) {
    LOGGING(LOG_ERROR, "scenic rhi2d: no OGR layers in %s", path.c_str());
    return false;
  }
  map_.CalEnvelope();
  return true;
}

bool ScenicRhi2dHost::ensure_device() {
  if (device_ && device_->renderer.GetDevice()) {
    return true;
  }
  device_ = std::make_unique<DeviceState>();
  const char* port = base::switch_cstr("rhi2d-port");
  const char* api = api_for_port(port);
  if (device_->renderer.CreateDevice(api) != scenic::detail::kErrNone ||
      !device_->renderer.GetDevice()) {
    LOGGING(LOG_ERROR, "scenic rhi2d: CreateDevice(%s) failed", api);
    device_.reset();
    return false;
  }
  return true;
}

bool ScenicRhi2dHost::attach(HWND hwnd, int width_px, int height_px) {
  if (!hwnd || width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (!ensure_map() || !ensure_device()) {
    return false;
  }
  scenic::detail::LPRENDERDEVICE dev = device_->renderer.GetDevice();
  if (hwnd_ != hwnd) {
    if (dev->host().init(hwnd, "map2d-scenic-rhi2d") !=
        scenic::detail::kErrNone) {
      LOGGING(LOG_ERROR, "scenic rhi2d: host.init failed hwnd=%p", hwnd);
      return false;
    }
    scenic::detail::RenderOptions2d options = {};
    options.show_mbr = false;
    options.show_point = true;
    options.point_radius = 3;
    dev->host().set_render_options(options);
    hwnd_ = hwnd;
    width_px_ = 0;
    height_px_ = 0;
  }
  if (width_px_ != width_px || height_px_ != height_px) {
    if (dev->host().resize(0, 0, width_px, height_px) !=
        scenic::detail::kErrNone) {
      LOGGING(LOG_ERROR, "scenic rhi2d: resize %dx%d failed", width_px,
              height_px);
      return false;
    }
    width_px_ = width_px;
    height_px_ = height_px;
  }
  return true;
}

bool ScenicRhi2dHost::apply_view(int width_px, int height_px,
                                 const ViewFrame* frame) {
  scenic::detail::LPRENDERDEVICE dev = device_->renderer.GetDevice();
  if (!dev) {
    return false;
  }
  content::Extent2 e = kChinaMap2dFrameExtent;
  e.ymin = 16.0;
  e.ymax = 52.0;
  if (frame) {
    const content::Extent2 v = frame->view_world_extent(width_px, height_px);
    if (extent_nonempty(v)) {
      e = v;
    }
  }
  scenic::detail::fRect frt;
  frt.lb.x = static_cast<float>(e.xmin);
  frt.lb.y = static_cast<float>(e.ymin);
  frt.rt.x = static_cast<float>(e.xmax);
  frt.rt.y = static_cast<float>(e.ymax);
  return dev->interact().zoom_to_rect(&map_, frt, true) ==
         scenic::detail::kErrNone;
}

bool ScenicRhi2dHost::refresh_frame(int width_px, int height_px,
                                    const ViewFrame* frame,
                                    bool settle_frame_job) {
  if (!is_ready()) {
    return false;
  }
  scenic::detail::LPRENDERDEVICE dev = device_->renderer.GetDevice();
  if (width_px_ != width_px || height_px_ != height_px) {
    if (dev->host().resize(0, 0, width_px, height_px) !=
        scenic::detail::kErrNone) {
      last_ok_ = false;
      return false;
    }
    width_px_ = width_px;
    height_px_ = height_px;
  }
  const uint64_t gen0 = dev->present().published_generation();
  if (!apply_view(width_px, height_px, frame)) {
    last_ok_ = false;
    return false;
  }
  if (dev->present().refresh() != scenic::detail::kErrNone) {
    last_ok_ = false;
    return false;
  }
  (void)dev->host().on_timer();
  if (settle_frame_job) {
    // zoom_to_rect after a painted HWND baseline stages an async FrameJob.
    // MAP save_image before publish is the ocean-clear key (170,211,223).
    // Match scenic_gdi_map_paint_test: pump OnTimer until generation moves.
    for (int i = 0; i < 400; ++i) {
      (void)dev->host().on_timer();
      if (dev->present().published_generation() > gen0) {
        for (int j = 0; j < 8; ++j) {
          (void)dev->host().on_timer();
          ::Sleep(1);
        }
        break;
      }
      ::Sleep(1);
    }
  }
  last_ok_ = true;
  return true;
}

bool ScenicRhi2dHost::paint_hdc(HDC hdc, int width_px, int height_px,
                                const ViewFrame* frame) {
  if (!hdc || !refresh_frame(width_px, height_px, frame,
                             /*settle_frame_job=*/false)) {
    last_ok_ = false;
    return false;
  }
  scenic::detail::LPRENDERDEVICE dev = device_->renderer.GetDevice();
  last_ok_ = dev->present().blit_to_dc(hdc) == scenic::detail::kErrNone;
  return last_ok_;
}

bool bmp_is_ocean_clear(const char* path) {
  if (!path || path[0] == '\0') {
    return false;
  }
  HBITMAP bmp =
      static_cast<HBITMAP>(LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0,
                                      LR_LOADFROMFILE | LR_CREATEDIBSECTION));
  if (!bmp) {
    return false;
  }
  BITMAP bm = {};
  GetObject(bmp, sizeof(bm), &bm);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bm.bmWidth;
  bmi.bmiHeader.biHeight = -bm.bmHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  const int pixels = bm.bmWidth * bm.bmHeight;
  if (pixels <= 0) {
    DeleteObject(bmp);
    return false;
  }
  std::vector<std::uint32_t> bits(static_cast<size_t>(pixels));
  HDC hdc = GetDC(nullptr);
  const int got = GetDIBits(hdc, bmp, 0, static_cast<UINT>(bm.bmHeight),
                            bits.data(), &bmi, DIB_RGB_COLORS);
  ReleaseDC(nullptr, hdc);
  DeleteObject(bmp);
  if (got <= 0) {
    return false;
  }
  int ocean = 0;
  int samples = 0;
  for (int i = 0; i < pixels; i += 16) {
    const unsigned b = bits[static_cast<size_t>(i)] & 0xffu;
    const unsigned g = (bits[static_cast<size_t>(i)] >> 8) & 0xffu;
    const unsigned r = (bits[static_cast<size_t>(i)] >> 16) & 0xffu;
    ++samples;
    if (std::abs(static_cast<int>(r) - 170) <= 8 &&
        std::abs(static_cast<int>(g) - 211) <= 8 &&
        std::abs(static_cast<int>(b) - 223) <= 8) {
      ++ocean;
    }
  }
  return samples > 0 &&
         (static_cast<float>(ocean) / static_cast<float>(samples)) > 0.92f;
}

bool ScenicRhi2dHost::export_bmp(const std::string& path, int width_px,
                                 int height_px, const ViewFrame* frame) {
  if (path.empty() ||
      !refresh_frame(width_px, height_px, frame, /*settle_frame_job=*/true)) {
    last_ok_ = false;
    return false;
  }
  scenic::detail::LPRENDERDEVICE dev = device_->renderer.GetDevice();
  if (dev->present().save_image(path.c_str(), false) !=
      scenic::detail::kErrNone) {
    last_ok_ = false;
    return false;
  }
  if (map2d_bmp_is_ocean_clear(path.c_str())) {
    LOGGING(LOG_ERROR,
            "scenic rhi2d: MAP save is ocean-clear after settle; "
            "caller should fall back to Map2dEngine");
    last_ok_ = false;
    return false;
  }
  last_ok_ = true;
  return true;
}

void ScenicRhi2dHost::ensure_draw_engine() {
  if (!draw_engine_) {
    draw_engine_.reset(scenic::create_map2d_engine());
  }
}

void ScenicRhi2dHost::shutdown_draw_engine() {
  if (draw_engine_) {
    draw_engine_->shutdown();
    draw_engine_.reset();
  }
}

bool ScenicRhi2dHost::hosts_draw_engine() const {
  return draw_engine_ != nullptr;
}

bool ScenicRhi2dHost::draw_engine_last_ok() const {
  return draw_engine_ ? draw_engine_->last_present_ok() : false;
}

bool ScenicRhi2dHost::export_draw_engine_bmp(const MapScene* scene,
                                             const ViewFrame* frame,
                                             const std::string& path,
                                             int width_px, int height_px) {
  ensure_draw_engine();
  if (!draw_engine_) {
    return false;
  }
  scenic::SessionDesc desc;
  desc.width_px = static_cast<uint32_t>(width_px);
  desc.height_px = static_cast<uint32_t>(height_px);
  draw_engine_->initialize(desc);
  const double scale = frame ? frame->scale() : 1.0;
  std::vector<scenic::Vertex2> xy;
  std::vector<scenic::DrawItem> items;
  std::fprintf(stderr, "scenic-export: fill begin scene=%p\n",
               static_cast<const void*>(scene));
  fill_scenic_draw_items(scene, scale, &xy, &items);
  std::fprintf(stderr, "scenic-export: fill done items=%zu verts=%zu\n",
               items.size(), xy.size());
  {
    const std::string step = path + ".scenic-step";
    if (FILE* tf = nullptr; fopen_s(&tf, step.c_str(), "a") == 0 && tf) {
      std::fprintf(tf, "fill items=%zu verts=%zu\n", items.size(), xy.size());
      std::fclose(tf);
    }
  }
  draw_engine_->bind_view(scenic_view_from_frame(frame));
  draw_engine_->bind_draw_items(items.data(),
                                static_cast<uint32_t>(items.size()));
  std::fprintf(stderr, "scenic-export: paint %dx%d path=%s\n", width_px,
               height_px, path.c_str());
  const bool ok = draw_engine_->export_bmp(path.c_str(),
                                           static_cast<uint32_t>(width_px),
                                           static_cast<uint32_t>(height_px));
  std::fprintf(stderr, "scenic-export: bmp ok=%d\n", ok ? 1 : 0);
  return ok;
}

}  // namespace detail
}  // namespace content
