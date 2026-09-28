// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/showcase/atmosphere_showcase.h"

#include <windows.h>
#include <shellapi.h>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/self_test/self_test.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <cwctype>

namespace {

using app::AtmosphereShowcaseMode;
using app::atmosphere_showcase_name;

void showcase_mark(const char* step);
void self_test_detach_maps(app::Browser& browser);

#ifndef PW_CLIENTONLY
#define PW_CLIENTONLY 0x00000001
#endif
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

constexpr uint32_t kAtmosphereShowcaseW = 640;
constexpr uint32_t kAtmosphereShowcaseH = 480;
constexpr wchar_t kAtmosphereShowcaseClass[] = L"SmartGisAtmosphereShowcase";

LRESULT CALLBACK atmosphere_showcase_wnd_proc(HWND hwnd, UINT msg, WPARAM wp,
                                              LPARAM lp) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      return 0;
    }
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

// Top-level present surface so FlyCube is not stuck on a tiny tab child HWND.
HWND create_atmosphere_showcase_hwnd(uint32_t width_px, uint32_t height_px) {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = atmosphere_showcase_wnd_proc;
  wc.hInstance = inst;
  wc.lpszClassName = kAtmosphereShowcaseClass;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassExW(&wc);

  RECT wr = {0, 0, static_cast<LONG>(width_px), static_cast<LONG>(height_px)};
  AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
  HWND hwnd = CreateWindowExW(
      WS_EX_APPWINDOW, kAtmosphereShowcaseClass,
      L"SmartGIS Atmosphere Showcase",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT,
      CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr,
      inst, nullptr);
  if (!hwnd) {
    return nullptr;
  }
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  SetForegroundWindow(hwnd);
  return hwnd;
}

// Linger policy for GPU showcase.
// GPU always stays until the present HWND is closed (sticky LINGER_MS must not
// auto-kill the window). Opt-in timed CI: SMT_ATMOSPHERE_SHOWCASE_TIMED_MS>0.
// SMT_ATMOSPHERE_SHOWCASE_LINGER_MS=0 skips linger (capture-only).
struct AtmosphereShowcaseLinger {
  bool until_close = false;
  DWORD ms = 0;
};

AtmosphereShowcaseLinger atmosphere_showcase_linger(bool want_gpu) {
  AtmosphereShowcaseLinger out;
  if (!want_gpu) {
    return out;
  }
  if (const char* timed = std::getenv("SMT_ATMOSPHERE_SHOWCASE_TIMED_MS")) {
    const int v = std::atoi(timed);
    if (v > 0) {
      out.ms = static_cast<DWORD>(v);
      return out;
    }
  }
  if (const char* env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_LINGER_MS")) {
    // Only "0" is honored; positive values are ignored so leftover shell env
    // cannot force a flash-and-exit.
    if (std::strcmp(env, "0") == 0) {
      return out;
    }
  }
  out.until_close = true;
  return out;
}

bool pixels_have_visible_signal(const unsigned char* pixels, int stride,
                                int width_px, int height_px) {
  if (!pixels || width_px < 32 || height_px < 32 || stride < width_px * 3) {
    return false;
  }
  // Sample a grid; require enough non-near-black samples (DXGI GDI-black bug).
  int lit = 0;
  int samples = 0;
  const int step_x = (std::max)(1, width_px / 32);
  const int step_y = (std::max)(1, height_px / 24);
  for (int y = 0; y < height_px; y += step_y) {
    const unsigned char* row = pixels + static_cast<size_t>(y) * stride;
    for (int x = 0; x < width_px; x += step_x) {
      const unsigned char b = row[x * 3 + 0];
      const unsigned char g = row[x * 3 + 1];
      const unsigned char r = row[x * 3 + 2];
      ++samples;
      if (static_cast<int>(r) + g + b > 24) {
        ++lit;
      }
    }
  }
  return samples > 0 && lit * 20 >= samples;  // >=5% lit
}

bool bmp_file_has_visible_signal(const wchar_t* filename, int* out_w,
                                 int* out_h) {
  if (!filename) {
    return false;
  }
  FILE* in = nullptr;
  if (_wfopen_s(&in, filename, L"rb") != 0 || !in) {
    return false;
  }
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER bi{};
  if (std::fread(&fh, sizeof(fh), 1, in) != 1 ||
      std::fread(&bi, sizeof(bi), 1, in) != 1 || fh.bfType != 0x4D42) {
    std::fclose(in);
    return false;
  }
  const int w = bi.biWidth;
  const int h = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (w < 320 || h < 240 || bi.biBitCount != 24) {
    std::fclose(in);
    return false;
  }
  const int stride = ((w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(h));
  if (std::fseek(in, static_cast<long>(fh.bfOffBits), SEEK_SET) != 0 ||
      std::fread(pixels.data(), 1, pixels.size(), in) != pixels.size()) {
    std::fclose(in);
    return false;
  }
  std::fclose(in);
  if (!pixels_have_visible_signal(pixels.data(), stride, w, h)) {
    return false;
  }
  // Reject flat clear-color frames (geometry never reached the swapchain).
  int unique = 0;
  unsigned char seen[64][3] = {};
  const int step_x = (std::max)(1, w / 24);
  const int step_y = (std::max)(1, h / 18);
  for (int y = 0; y < h; y += step_y) {
    const unsigned char* row = pixels.data() + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; x += step_x) {
      const unsigned char b = row[x * 3 + 0];
      const unsigned char g = row[x * 3 + 1];
      const unsigned char r = row[x * 3 + 2];
      bool found = false;
      for (int i = 0; i < unique; ++i) {
        if (seen[i][0] == r && seen[i][1] == g && seen[i][2] == b) {
          found = true;
          break;
        }
      }
      if (!found && unique < 64) {
        seen[unique][0] = r;
        seen[unique][1] = g;
        seen[unique][2] = b;
        ++unique;
      }
    }
  }
  // Solid DEM is often clear + one mesh color; ocean/cloud add more.
  // Reject pure clear (unique==1).
  return unique >= 2;
}

bool blit_client_to_dib(HWND hwnd, HDC mem, int w, int h) {
  // Screen BitBlt sees DWM-composited DXGI content; PrintWindow often does not.
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  POINT origin = {0, 0};
  ClientToScreen(hwnd, &origin);
  const BOOL ok = BitBlt(mem, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY);
  ReleaseDC(nullptr, screen);
  return ok != FALSE;
}

bool capture_hwnd_bmp(HWND hwnd, const wchar_t* filename) {
  if (!hwnd || !IsWindow(hwnd) || !filename) {
    return false;
  }
  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    return false;
  }
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w < 8 || h < 8) {
    return false;
  }
  HDC wnd_dc = GetDC(hwnd);
  if (!wnd_dc) {
    return false;
  }
  HDC mem = CreateCompatibleDC(wnd_dc);
  HBITMAP bmp = CreateCompatibleBitmap(wnd_dc, w, h);
  if (!mem || !bmp) {
    if (bmp) {
      DeleteObject(bmp);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(hwnd, wnd_dc);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, bmp);

  // Prefer full-content PrintWindow; fall back to desktop BitBlt for DXGI.
  BOOL printed =
      PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT | PW_CLIENTONLY);
  if (!printed) {
    printed = PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
  }
  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = w;
  bi.biHeight = -h;  // top-down
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  const int stride = ((w * 3 + 3) / 4) * 4;
  std::vector<unsigned char> pixels(static_cast<size_t>(stride) *
                                    static_cast<size_t>(h));
  int got = GetDIBits(mem, bmp, 0, h, pixels.data(),
                      reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
  if (got != h ||
      !pixels_have_visible_signal(pixels.data(), stride, w, h)) {
    if (blit_client_to_dib(hwnd, mem, w, h)) {
      got = GetDIBits(mem, bmp, 0, h, pixels.data(),
                      reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    }
  }

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, wnd_dc);
  if (got != h) {
    return false;
  }

  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size());

  FILE* out = nullptr;
  if (_wfopen_s(&out, filename, L"wb") != 0 || !out) {
    return false;
  }
  std::fwrite(&fh, sizeof(fh), 1, out);
  std::fwrite(&bi, sizeof(bi), 1, out);
  std::fwrite(pixels.data(), 1, pixels.size(), out);
  std::fclose(out);
  return true;
}

int run_atmosphere_showcase_impl(app::Browser& browser,
                            AtmosphereShowcaseMode mode) {
  const char* name = atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  showcase_mark(name);

  browser.select_map_tab(2);
  showcase_mark("tab3d");
  app::pump_views_messages(600);
  showcase_mark("pumped");
  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    std::fprintf(stderr, "atmosphere-showcase: 3D viewport HWND missing\n");
    self_test_detach_maps(browser);
    return 50;
  }
  showcase_mark("scene-hwnd-ok");

  // Default: Null RHI (deterministic exit). Set SMT_ATMOSPHERE_SHOWCASE_GPU=1
  // for FlyCube/DX12 on a dedicated 640x480 present window (not the tiny tab
  // child). GPU lingers until the present HWND is closed; CI may set
  // SMT_ATMOSPHERE_SHOWCASE_TIMED_MS, or LINGER_MS=0 to skip.
  const bool want_gpu = []() {
    if (const char* env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_GPU")) {
      return env[0] == '1' && env[1] == '\0';
    }
    return false;
  }();
  const AtmosphereShowcaseLinger linger =
      atmosphere_showcase_linger(want_gpu);

  // Drop any ContentMapView / prior FlyCube on the tab before we own a device.
  if (scene->attach_mode() ==
          ui::views::MapViewport::AttachMode::kContentMapView ||
      scene->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube) {
    scene->detach();
    showcase_mark("detached");
    app::pump_views_messages(100);
  }

  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  if (want_gpu) {
    owned_present_hwnd =
        create_atmosphere_showcase_hwnd(kAtmosphereShowcaseW,
                                        kAtmosphereShowcaseH);
    if (!owned_present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: present HWND create failed\n");
      self_test_detach_maps(browser);
      return 50;
    }
    present_hwnd = owned_present_hwnd;
    showcase_mark("present-hwnd-ok");
  } else {
    present_hwnd = scene->native_view();
    if (!present_hwnd) {
      scene->realize_native();
      present_hwnd = scene->native_view();
    }
    if (!present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: HWND gone after detach\n");
      self_test_detach_maps(browser);
      return 50;
    }
  }
  showcase_mark("hwnd-ready");

  render::rhi::Device* device = render::rhi::create_device(
      want_gpu ? render::rhi::preferred_gpu_backend()
               : render::rhi::Backend::kNull);
  if (!device) {
    std::fprintf(stderr, "atmosphere-showcase: create_device failed\n");
    showcase_mark("device-missing");
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    self_test_detach_maps(browser);
    return 51;
  }
  showcase_mark("device-created");
  const bool owns_device = true;

  render::rhi::DeviceDesc desc;
  desc.native_window = want_gpu ? present_hwnd : nullptr;
  desc.width = kAtmosphereShowcaseW;
  desc.height = kAtmosphereShowcaseH;
  std::fprintf(stderr,
               "atmosphere-showcase: gpu=%d linger=%s present=%p %ux%u\n",
               want_gpu ? 1 : 0,
               linger.until_close ? "until-close"
                                  : (linger.ms > 0 ? "timed" : "none"),
               static_cast<void*>(present_hwnd), desc.width, desc.height);
  if (!linger.until_close && linger.ms > 0) {
    std::fprintf(stderr, "atmosphere-showcase: linger_ms=%lu\n",
                 static_cast<unsigned long>(linger.ms));
  }
  if (!device->initialize(desc)) {
    std::fprintf(stderr, "atmosphere-showcase: device initialize failed\n");
    device->shutdown();
    showcase_mark("device-missing");
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    self_test_detach_maps(browser);
    return 51;
  }
  if (want_gpu) {
    if (render::rhi::CommandList* warm = device->create_command_list()) {
      render::rhi::RenderPassDesc pass;
      pass.clear_r = 0.05f;
      pass.clear_g = 0.12f;
      pass.clear_b = 0.18f;
      pass.clear_a = 1.f;
      pass.width = desc.width;
      pass.height = desc.height;
      warm->begin_render_pass(pass);
      warm->set_viewport(0, 0, static_cast<float>(desc.width),
                         static_cast<float>(desc.height), 0, 1);
      warm->end_render_pass();
      warm->close();
      device->execute(warm);
      device->destroy_command_list(warm);
      device->present();
    }
  }
  showcase_mark(want_gpu ? "device-init-gpu" : "device-init-null");
  showcase_mark(want_gpu ? "flycube-ok" : "null-ok");

  app::Scene3dPresenter* cam = browser.scene3d();
  app::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    self_test_detach_maps(browser);
    return 50;
  }

  // Distinct camera nudge so frames are not the default identity orbit.
  orbit->apply_pan(40, -18);
  orbit->apply_wheel_at(320, 240, 120, static_cast<int>(kAtmosphereShowcaseW),
                        static_cast<int>(kAtmosphereShowcaseH));

  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      // Leave Environment unset — DEM / land present only.
      break;
    case AtmosphereShowcaseMode::kOcean:
      cam->seed_atmosphere_procedural();
      cam->set_ocean_enabled(true);
      cam->set_cloud_enabled(false);
      break;
    case AtmosphereShowcaseMode::kFull:
      cam->enable_atmosphere_demo();
      break;
    case AtmosphereShowcaseMode::kCoast: {
      // East China Sea coastal window — different extent from full China.
      const content::Extent2 coast{118.0, 28.0, 128.0, 36.0};
      orbit->apply_world_extent(coast);
      cam->enable_atmosphere_demo();
      break;
    }
    case AtmosphereShowcaseMode::kNone:
    default:
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      self_test_detach_maps(browser);
      return 53;
  }

  const gis::atmosphere::Environment* env = cam->atmosphere();
  const bool want_ocean =
      mode == AtmosphereShowcaseMode::kOcean ||
      mode == AtmosphereShowcaseMode::kFull ||
      mode == AtmosphereShowcaseMode::kCoast;
  const bool want_cloud = mode == AtmosphereShowcaseMode::kFull ||
                           mode == AtmosphereShowcaseMode::kCoast;
  // Full/coast demos enable the full geo-aligned stack (sky+fog too).
  const bool want_sky = want_cloud;
  const bool want_fog = want_cloud;
  if (mode == AtmosphereShowcaseMode::kLand) {
    if (env && (env->ocean_enabled() || env->cloud_enabled() ||
                env->sky_enabled() || env->fog_enabled())) {
      std::fprintf(stderr,
                   "atmosphere-showcase: land mode still has passes on\n");
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      self_test_detach_maps(browser);
      return 53;
    }
  } else {
    if (!env) {
      std::fprintf(stderr, "atmosphere-showcase: Environment missing\n");
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      self_test_detach_maps(browser);
      return 53;
    }
    if (env->ocean_enabled() != want_ocean ||
        env->cloud_enabled() != want_cloud ||
        env->sky_enabled() != want_sky || env->fog_enabled() != want_fog) {
      std::fprintf(stderr,
                   "atmosphere-showcase: flag mismatch ocean=%d cloud=%d "
                   "sky=%d fog=%d (want %d/%d/%d/%d)\n",
                   env->ocean_enabled() ? 1 : 0, env->cloud_enabled() ? 1 : 0,
                   env->sky_enabled() ? 1 : 0, env->fog_enabled() ? 1 : 0,
                   want_ocean ? 1 : 0, want_cloud ? 1 : 0, want_sky ? 1 : 0,
                   want_fog ? 1 : 0);
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      self_test_detach_maps(browser);
      return 53;
    }
    if (env->field_store().layer_count() == 0) {
      std::fprintf(stderr, "atmosphere-showcase: FieldStore empty\n");
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      self_test_detach_maps(browser);
      return 53;
    }
  }
  showcase_mark("config-ok");
  std::fprintf(stderr, "atmosphere-showcase: ocean=%d cloud=%d layers=%zu\n",
               env && env->ocean_enabled() ? 1 : 0,
               env && env->cloud_enabled() ? 1 : 0,
               env ? env->field_store().layer_count() : 0u);

  const uint32_t kW = kAtmosphereShowcaseW;
  const uint32_t kH = kAtmosphereShowcaseH;
  int presents = 0;
  auto present_one = [&](const char* mark) -> bool {
    showcase_mark(mark);
    if (!cam->present_gpu(device, kW, kH)) {
      return false;
    }
    ++presents;
    app::pump_views_messages(50);
    return true;
  };
  for (int i = 0; i < 3; ++i) {
    char frame_mark[32];
    std::snprintf(frame_mark, sizeof(frame_mark), "present-%d", i);
    if (!present_one(frame_mark)) {
      std::fprintf(stderr, "atmosphere-showcase: present_gpu failed frame %d\n",
                   i);
      cam->abandon_mesh();
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      self_test_detach_maps(browser);
      return 52;
    }
  }
  showcase_mark("present-ok");
  std::fprintf(stderr, "atmosphere-showcase: presented %d frames %ux%u\n",
               presents, kW, kH);

  // Interactive linger: keep presenting so ocean/cloud stay visible.
  // Capture while the present HWND is still alive (before until-close ends).
  auto capture_showcase_bmp = [&]() -> bool {
    wchar_t bmp_path[MAX_PATH] = {};
    wchar_t file[64] = {};
    swprintf_s(file, L"atmosphere-showcase-%S.bmp", name);
    if (!app::detail::exe_sidecar_path(bmp_path, MAX_PATH, file)) {
      return !want_gpu;
    }
    HWND capture_hwnd =
        owned_present_hwnd ? owned_present_hwnd : present_hwnd;
    (void)cam->present_gpu(device, kW, kH);
    app::pump_views_messages(80);
    if (!capture_hwnd_bmp(capture_hwnd, bmp_path)) {
      showcase_mark("bmp-skip");
      std::fprintf(stderr, "atmosphere-showcase: BMP capture skipped\n");
      return !want_gpu;
    }
    int bw = 0;
    int bh = 0;
    const bool signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh);
    std::fwprintf(stderr,
                  L"atmosphere-showcase: wrote %ls (%dx%d signal=%d)\n",
                  bmp_path, bw, bh, signal ? 1 : 0);
    if (signal) {
      showcase_mark("bmp-ok");
      return true;
    }
    showcase_mark("bmp-black");
    std::fprintf(stderr, "atmosphere-showcase: BMP lacks visible signal\n");
    return false;
  };

  bool bmp_signal_ok = !want_gpu;
  if (linger.until_close || linger.ms > 0) {
    showcase_mark("linger-start");
    if (linger.until_close) {
      std::fprintf(stderr,
                   "atmosphere-showcase: linger until window closed "
                   "(close the showcase window when done)\n");
    } else {
      std::fprintf(stderr, "atmosphere-showcase: linger %lu ms\n",
                   static_cast<unsigned long>(linger.ms));
    }
    const DWORD linger_end =
        linger.until_close ? 0u : (GetTickCount() + linger.ms);
    int linger_frames = 0;
    bool captured = false;
    for (;;) {
      if (owned_present_hwnd && !IsWindow(owned_present_hwnd)) {
        break;
      }
      if (!linger.until_close && GetTickCount() >= linger_end) {
        break;
      }
      if (!cam->present_gpu(device, kW, kH)) {
        std::fprintf(stderr, "atmosphere-showcase: linger present failed\n");
        break;
      }
      ++linger_frames;
      if (!captured && linger_frames >= 8) {
        bmp_signal_ok = capture_showcase_bmp();
        captured = true;
      }
      app::pump_views_messages(33);
    }
    if (!captured) {
      bmp_signal_ok = capture_showcase_bmp();
    }
    showcase_mark("linger-ok");
    std::fprintf(stderr, "atmosphere-showcase: linger frames=%d\n",
                 linger_frames);
  } else if (want_gpu) {
    bmp_signal_ok = capture_showcase_bmp();
  }

  cam->abandon_mesh();
  if (owns_device && device) {
    device->shutdown();
    // Intentionally leak Device* — FlyCube teardown has corrupted heaps
    // when operator delete runs after a live DX12 session (see MapViewport).
  }
  if (owned_present_hwnd) {
    DestroyWindow(owned_present_hwnd);
    owned_present_hwnd = nullptr;
  }
  self_test_detach_maps(browser);
  if (want_gpu && !bmp_signal_ok) {
    showcase_mark("bmp-fail");
    std::fprintf(stderr, "atmosphere-showcase: FAIL mode=%s (exit 54)\n", name);
    return 54;
  }
  showcase_mark("pass");
  std::fprintf(stderr, "atmosphere-showcase: PASS mode=%s\n", name);
  return 0;
}

void showcase_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  if (!app::detail::exe_sidecar_path(path, MAX_PATH,
                                     L"atmosphere-showcase-mark.txt")) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

void self_test_detach_maps(app::Browser& browser) {
  if (browser.scene3d()) {
    browser.scene3d()->abandon_mesh();
  }
  if (ui::views::MapViewport* m = browser.map_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_data_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_scene_viewport()) {
    m->detach();
  }
}

}  // namespace

namespace app {

int run_atmosphere_showcase(Browser& browser, AtmosphereShowcaseMode mode) {
  return run_atmosphere_showcase_impl(browser, mode);
}

}  // namespace app
