// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Views host PE. Browser chrome is app::BrowserView; this file only
// dispatches ContentMain (--type=gpu / --type=renderer) and --self-test.

#include <windows.h>
#include <shellapi.h>

#include "app/views/browser_view.h"
#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "content/public/events.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/map_widget_host_view.h"
#include "content/public/view_host.h"
#include "gpu/gpu.h"
#include "render/rhi/rhi.h"
#include "tool/interaction.h"
#include "tool/workspace.h"
#include "ui/views/gis/catalog_view.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/kernel/layout_check.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu_bar.h"
#include "ui/views/gis/status_bar.h"
#include "ui/views/kernel/view.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <cwctype>

namespace {

bool cmd_has_self_test() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

// Automated 3D atmosphere demos (distinct from full --self-test chrome path).
// Modes: land | ocean | full | coast
enum class AtmosphereShowcaseMode {
  kNone,
  kLand,
  kOcean,
  kFull,
  kCoast,
};

AtmosphereShowcaseMode parse_atmosphere_showcase() {
  const wchar_t* cmd = GetCommandLineW();
  if (!cmd) {
    return AtmosphereShowcaseMode::kNone;
  }
  // Accept both `--atmosphere-showcase=land` and `--atmosphere-showcase land`
  // (cmd.exe may treat '=' as a token separator).
  const wchar_t* p = wcsstr(cmd, L"--atmosphere-showcase");
  if (!p) {
    return AtmosphereShowcaseMode::kNone;
  }
  p += wcslen(L"--atmosphere-showcase");
  while (*p == L' ' || *p == L'\t' || *p == L'=') {
    ++p;
  }
  if (wcsncmp(p, L"land", 4) == 0 && (p[4] == 0 || iswspace(p[4]) || p[4] == L'"')) {
    return AtmosphereShowcaseMode::kLand;
  }
  if (wcsncmp(p, L"ocean", 5) == 0 &&
      (p[5] == 0 || iswspace(p[5]) || p[5] == L'"')) {
    return AtmosphereShowcaseMode::kOcean;
  }
  if (wcsncmp(p, L"full", 4) == 0 && (p[4] == 0 || iswspace(p[4]) || p[4] == L'"')) {
    return AtmosphereShowcaseMode::kFull;
  }
  if (wcsncmp(p, L"coast", 5) == 0 &&
      (p[5] == 0 || iswspace(p[5]) || p[5] == L'"')) {
    return AtmosphereShowcaseMode::kCoast;
  }
  return AtmosphereShowcaseMode::kNone;
}

const char* atmosphere_showcase_name(AtmosphereShowcaseMode mode) {
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      return "land";
    case AtmosphereShowcaseMode::kOcean:
      return "ocean";
    case AtmosphereShowcaseMode::kFull:
      return "full";
    case AtmosphereShowcaseMode::kCoast:
      return "coast";
    case AtmosphereShowcaseMode::kNone:
    default:
      return "none";
  }
}

void pump_briefly(DWORD ms);
void self_test_detach_maps(app::BrowserView& browser);

void showcase_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      break;
    }
  }
  if (wcscat_s(path, L"atmosphere-showcase-mark.txt") != 0) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

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

int run_atmosphere_showcase(app::BrowserView& browser,
                            AtmosphereShowcaseMode mode) {
  const char* name = atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  showcase_mark(name);

  browser.select_map_tab(2);
  showcase_mark("tab3d");
  pump_briefly(600);
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
    pump_briefly(100);
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

  app::Scene3dController* cam = browser.scene3d();
  if (!cam) {
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    self_test_detach_maps(browser);
    return 50;
  }

  // Distinct camera nudge so frames are not the default identity orbit.
  cam->apply_pan(40, -18);
  cam->apply_wheel_at(320, 240, 120, static_cast<int>(kAtmosphereShowcaseW),
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
      cam->apply_world_extent(coast);
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
  if (mode == AtmosphereShowcaseMode::kLand) {
    if (env && (env->ocean_enabled() || env->cloud_enabled())) {
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
        env->cloud_enabled() != want_cloud) {
      std::fprintf(stderr,
                   "atmosphere-showcase: flag mismatch ocean=%d cloud=%d "
                   "(want %d/%d)\n",
                   env->ocean_enabled() ? 1 : 0, env->cloud_enabled() ? 1 : 0,
                   want_ocean ? 1 : 0, want_cloud ? 1 : 0);
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
    pump_briefly(50);
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
    if (GetModuleFileNameW(nullptr, bmp_path, MAX_PATH) == 0) {
      return !want_gpu;
    }
    for (int i = static_cast<int>(wcslen(bmp_path)) - 1; i >= 0; --i) {
      if (bmp_path[i] == L'\\' || bmp_path[i] == L'/') {
        bmp_path[i + 1] = L'\0';
        break;
      }
    }
    wchar_t file[64] = {};
    swprintf_s(file, L"atmosphere-showcase-%S.bmp", name);
    if (wcscat_s(bmp_path, file) != 0) {
      return !want_gpu;
    }
    HWND capture_hwnd =
        owned_present_hwnd ? owned_present_hwnd : present_hwnd;
    (void)cam->present_gpu(device, kW, kH);
    pump_briefly(80);
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
      pump_briefly(33);
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

void pump_briefly(DWORD ms) {
  const DWORD end = GetTickCount() + ms;
  MSG msg;
  while (GetTickCount() < end) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
}

void self_test_mark(const char* step) {
  wchar_t path[MAX_PATH] = {};
  DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return;
  }
  // Write next to the exe (out/), independent of process cwd.
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      path[i + 1] = L'\0';
      break;
    }
  }
  if (wcscat_s(path, L"self-test-mark.txt") != 0) {
    return;
  }
  FILE* f = nullptr;
  if (_wfopen_s(&f, path, L"a") == 0 && f) {
    std::fprintf(f, "%s\n", step);
    std::fflush(f);
    std::fclose(f);
  }
}

void self_test_detach_maps(app::BrowserView& browser) {
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

bool viewport_has_presented_frame(ui::views::MapViewport* pane) {
  if (!pane ||
      pane->attach_mode() !=
          ui::views::MapViewport::AttachMode::kContentMapView) {
    return false;
  }
  content::MapContents* session = pane->map_contents();
  if (!session || pane->view_id() == 0) {
    return false;
  }
  content::MapWidgetHostView* view = session->HostView(pane->view_id());
  if (!view) {
    return false;
  }
  const content::SharedSurface surface = view->Latest();
  return surface.generation > 0 && surface.nt_handle != nullptr &&
         surface.width_px >= 8 && surface.height_px >= 8;
}


int BrowserMain(const content::ContentMainParams&) {
  ui::views::enable_process_dpi_awareness();
  const AtmosphereShowcaseMode showcase = parse_atmosphere_showcase();
  // Default Scene3d prefers FlyCube; DX12 multi-viewport attach can hang on
  // some hosts. Opt out with SMT_FORCE_CONTENT_MAPVIEW_3D=1 (or legacy
  // SMT_PREFER_FLYCUBE_3D=0) before BrowserView::init. Showcase acquires
  // FlyCube after the shell is up (see run_atmosphere_showcase).
  // --self-test forces ContentMapView so chrome smoke stays hang-free; product
  // interactive runs keep the FlyCube default.
  const bool self_test = cmd_has_self_test();
  // Showcase also forces ContentMapView: multi-viewport FlyCube attach during
  // BrowserView::init can hang; run_atmosphere_showcase opens its own present
  // HWND after the shell is up.
  if (self_test || showcase != AtmosphereShowcaseMode::kNone) {
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_3D", "1");
  }
  {
    wchar_t diag[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, diag, MAX_PATH) > 0) {
      for (int i = static_cast<int>(wcslen(diag)) - 1; i >= 0; --i) {
        if (diag[i] == L'\\' || diag[i] == L'/') {
          diag[i + 1] = L'\0';
          break;
        }
      }
      if (wcscat_s(diag, L"atmosphere-showcase-cmdline.txt") == 0) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, diag, L"w") == 0 && f) {
          const wchar_t* cmd = GetCommandLineW();
          std::fwprintf(f, L"cmd=%s\nshowcase=%hs\n", cmd ? cmd : L"(null)",
                        atmosphere_showcase_name(showcase));
          std::fclose(f);
        }
      }
    }
  }
  app::BrowserView browser;
  if (!browser.init()) {
    return 1;
  }
  browser.show();
  if (showcase != AtmosphereShowcaseMode::kNone) {
    wchar_t mark_path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, mark_path, MAX_PATH) > 0) {
      for (int i = static_cast<int>(wcslen(mark_path)) - 1; i >= 0; --i) {
        if (mark_path[i] == L'\\' || mark_path[i] == L'/') {
          mark_path[i + 1] = L'\0';
          break;
        }
      }
      wcscat_s(mark_path, L"atmosphere-showcase-mark.txt");
      DeleteFileW(mark_path);
    }
    pump_briefly(400);
    if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
      return 2;
    }
    return run_atmosphere_showcase(browser, showcase);
  }
  if (self_test) {
    wchar_t mark_path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, mark_path, MAX_PATH) > 0) {
      for (int i = static_cast<int>(wcslen(mark_path)) - 1; i >= 0; --i) {
        if (mark_path[i] == L'\\' || mark_path[i] == L'/') {
          mark_path[i + 1] = L'\0';
          break;
        }
      }
      wcscat_s(mark_path, L"self-test-mark.txt");
      DeleteFileW(mark_path);
    }
    self_test_mark("show");
    pump_briefly(400);
    if (!browser.hwnd() || !IsWindow(browser.hwnd())) {
      return 2;
    }
    self_test_mark("hwnd-ok");
    // 2D Map Edit: require a presented frame when content map hang is active.
    ui::views::MapViewport* map = browser.map_viewport();
    if (map &&
        map->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView) {
      if (!map->wait_ready(20000)) {
        return 3;
      }
      if (!viewport_has_presented_frame(map)) {
        return 3;
      }
      self_test_mark("map-frame-ok");
    }
    self_test_mark("map-ready");
    // Stop present timers on every map HWND before walking chrome. Do not
    // pump here: PeekMessage would deliver WM_PAINT and race Widget paint
    // buffers / MapViewport backbuffers (AV after map-ready).
    auto stop_present = [](ui::views::MapViewport* pane) {
      if (pane && pane->native_view() && IsWindow(pane->native_view())) {
        KillTimer(pane->native_view(), 1);  // kPresentTimerId == 1
      }
    };
    stop_present(map);
    stop_present(browser.map_data_viewport());
    stop_present(browser.map_scene_viewport());
    self_test_mark("post-map");
    ui::views::View* root = browser.contents_view();
    if (!root) {
      return 4;
    }
    self_test_mark("root-ok");
    if (root->child_count() < 3) {
      return 4;
    }
    self_test_mark("child-count-ok");
    ui::views::View* columns = root->child_at(1);
    if (!columns || columns->child_count() < 2) {
      return 5;
    }
    self_test_mark("columns-ok");
    ui::views::CatalogView* catalog = browser.catalog_view();
    if (!catalog || !catalog->layer_tree()) {
      return 6;
    }
    self_test_mark("catalog-ok");
    // 2D Data tab: same session; HWND must stay live after switch.
    browser.select_map_tab(1);
    pump_briefly(200);
    ui::views::MapViewport* data = browser.map_data_viewport();
    if (!data || !data->native_view() || !IsWindow(data->native_view())) {
      return 7;
    }
    // Inactive Map Edit HWND must hide so it does not cover the Data pane.
    if (map && map->native_view() && IsWindow(map->native_view()) &&
        IsWindowVisible(map->native_view())) {
      std::fprintf(stderr, "inactive map HWND still visible after Data tab\n");
      return 36;
    }
    if (!IsWindowVisible(data->native_view())) {
      std::fprintf(stderr, "active Data HWND not visible\n");
      return 37;
    }
    if (data->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView &&
        !data->wait_ready(20000)) {
      return 8;
    }
    self_test_mark("data-ready");
    // 3D Scene tab: activate view3d.trackball and require a frame when hung.
    browser.select_map_tab(2);
    pump_briefly(400);
    ui::views::MapViewport* scene = browser.map_scene_viewport();
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      return 9;
    }
    if (scene->attach_mode() ==
            ui::views::MapViewport::AttachMode::kContentMapView) {
      if (!scene->wait_ready(20000)) {
        return 10;
      }
      if (!viewport_has_presented_frame(scene)) {
        return 10;
      }
      self_test_mark("scene-frame-ok");
    }
    self_test_mark("scene-ready");
    // Placeholder / FlyCube / content are all acceptable; prove pan/orbit
    // input reaches ViewHost without crashing when no GPU is present.
    content::ViewHost* scene_host = scene->view_host();
    if (!scene_host || !scene_host->workspace()) {
      return 21;
    }
    if (!scene_host->activate("view3d.trackball")) {
      return 22;
    }
    tool::Interaction* scene_tool = scene_host->workspace()->stack().current();
    if (!scene_tool ||
        std::strcmp(scene_tool->id(), "view3d.trackball") != 0) {
      return 23;
    }
    content::InputEvent orbit_down{};
    orbit_down.kind = content::InputEvent::Kind::kLDown;
    orbit_down.x_px = 24;
    orbit_down.y_px = 30;
    content::InputEvent orbit_move{};
    orbit_move.kind = content::InputEvent::Kind::kMouseMove;
    orbit_move.x_px = 48;
    orbit_move.y_px = 52;
    content::InputEvent orbit_up{};
    orbit_up.kind = content::InputEvent::Kind::kLUp;
    orbit_up.x_px = 48;
    orbit_up.y_px = 52;
    if (!scene_host->dispatch_input(orbit_down) ||
        !scene_host->dispatch_input(orbit_move) ||
        !scene_host->dispatch_input(orbit_up)) {
      return 24;
    }
    if (!browser.scene3d() ||
        std::fabs(browser.scene3d()->yaw() - app::kScene3dDefaultYaw) <
            0.001f) {
      // Trackball drag must move the chrome 3D camera (not a static mesh).
      self_test_detach_maps(browser);
      return 25;
    }
    self_test_mark("orbit-ok");
    browser.select_map_tab(0);
    pump_briefly(100);
    content::ViewHost* host = browser.edit_view_host();
    if (!host || !host->workspace() || !host->edits()) {
      return 11;
    }
    if (!browser.run_tool_command("edit.append.point")) {
      return 12;
    }
    self_test_mark("edit-point");
    tool::Interaction* cur = host->workspace()->stack().current();
    if (!cur || std::strcmp(cur->id(), "draw.point") != 0) {
      return 13;
    }
    content::InputEvent down{};
    down.kind = content::InputEvent::Kind::kLDown;
    down.x_px = 12;
    down.y_px = 18;
    if (!host->dispatch_input(down)) {
      return 14;
    }
    if (!host->edits()->can_undo()) {
      return 15;
    }
    ui::views::StatusBar* status = browser.status_bar();
    if (!status || status->status().find("Committed") == std::string::npos) {
      return 16;
    }
    if (!browser.run_tool_command("selection.point")) {
      return 17;
    }
    cur = host->workspace()->stack().current();
    if (!cur || std::strcmp(cur->id(), "select.point") != 0) {
      return 18;
    }
    if (!browser.run_tool_command("selection.clear")) {
      return 19;
    }
    if (!status || status->status().find("Selection cleared") ==
                       std::string::npos) {
      return 20;
    }
    self_test_mark("selection-ok");
    if (!browser.run_tool_command("view.backend.maplibre")) {
      return 43;
    }
    if (!status || status->status().find("MapLibre") == std::string::npos) {
      return 44;
    }
    if (!browser.run_tool_command("view.backend.rhi")) {
      return 45;
    }
    if (!status || status->status().find("RHI") == std::string::npos) {
      return 46;
    }
    self_test_mark("backend-ok");
    // Document layers + features (Catalog / overlay paint).
    if (!browser.document() || browser.document()->layer_count() == 0) {
      self_test_detach_maps(browser);
      return 36;
    }
    if (browser.document()->feature_count() < 3) {
      self_test_detach_maps(browser);
      return 37;
    }
    if (ui::views::CatalogView* cat = browser.catalog_view()) {
      if (!cat->layer_tree() || cat->layer_tree()->layer_count() == 0) {
        self_test_detach_maps(browser);
        return 38;
      }
    }
    self_test_mark("layers-ok");
    // Prefer out/china_city.gpkg (鍥涘浘灞?; else geojson; else china_plp.
    {
      wchar_t sample_w[MAX_PATH] = {};
      if (GetModuleFileNameW(nullptr, sample_w, MAX_PATH) > 0) {
        for (int i = static_cast<int>(wcslen(sample_w)) - 1; i >= 0; --i) {
          if (sample_w[i] == L'\\' || sample_w[i] == L'/') {
            sample_w[i + 1] = L'\0';
            break;
          }
        }
        char sample_a[MAX_PATH] = {};
        bool opened = false;
        bool city_pack = false;
        const wchar_t* candidates[] = {L"china_city.gpkg",
                                       L"china_city.geojson",
                                       L"china_plp.geojson"};
        for (const wchar_t* name : candidates) {
          wchar_t china_w[MAX_PATH] = {};
          wcscpy_s(china_w, sample_w);
          wcscat_s(china_w, name);
          if (GetFileAttributesW(china_w) == INVALID_FILE_ATTRIBUTES) {
            continue;
          }
          WideCharToMultiByte(CP_UTF8, 0, china_w, -1, sample_a, MAX_PATH,
                              nullptr, nullptr);
          if (browser.document()->open_path(sample_a) &&
              browser.document()->last_open_was_ogr()) {
            opened = true;
            city_pack = (wcsstr(name, L"china_city") != nullptr);
            break;
          }
        }
        if (!opened) {
          wcscat_s(sample_w, L"views_ogr_selftest.geojson");
          FILE* sf = nullptr;
          if (_wfopen_s(&sf, sample_w, L"wb") == 0 && sf) {
            static const char kGeojson[] =
                "{\"type\":\"FeatureCollection\",\"name\":\"china_plp\","
                "\"features\":["
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"Beijing\","
                "\"kind\":\"point\"},"
                "\"geometry\":{\"type\":\"Point\",\"coordinates\":"
                "[116.3974,39.9093]}},"
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"Jingjin\","
                "\"kind\":\"line\"},"
                "\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
                "[[116.3974,39.9093],[116.7,39.7],[117.2,39.12]]}},"
                "{\"type\":\"Feature\",\"properties\":{\"name\":\"Huabei\","
                "\"kind\":\"area\"},"
                "\"geometry\":{\"type\":\"Polygon\",\"coordinates\":"
                "[[[116.2,39.7],[116.8,39.7],[116.8,40.1],[116.2,40.1],"
                "[116.2,39.7]]]}}"
                "]}";
            std::fwrite(kGeojson, 1, sizeof(kGeojson) - 1, sf);
            std::fclose(sf);
            WideCharToMultiByte(CP_UTF8, 0, sample_w, -1, sample_a, MAX_PATH,
                                nullptr, nullptr);
            opened = browser.document()->open_path(sample_a) &&
                     browser.document()->last_open_was_ogr();
          }
        }
        if (!opened || browser.document()->feature_count() < 3 ||
            browser.document()->layer_count() == 0) {
          std::fprintf(stderr, "OGR China map self-test open failed: %s\n",
                       sample_a);
          self_test_detach_maps(browser);
          return 26;
        }
        if (city_pack) {
          if (browser.document()->layer_count() < 4) {
            std::fprintf(stderr,
                         "china_city pack expected >=4 layers (area/line/"
                         "point/text), got %zu\n",
                         browser.document()->layer_count());
            self_test_detach_maps(browser);
            return 26;
          }
          if (browser.document()->feature_count() < 200) {
            std::fprintf(stderr,
                         "china_city pack expected >=200 features, got %zu\n",
                         browser.document()->feature_count());
            self_test_detach_maps(browser);
            return 26;
          }
        }
        if (!browser.document()->has_china_extent()) {
          std::fprintf(stderr, "OGR China extent not in China lon/lat\n");
          self_test_detach_maps(browser);
          return 39;
        }
        browser.catalog_view()->populate_layers([&] {
          std::vector<ui::views::LayerTree::LayerDesc> layers;
          for (const auto& d : browser.document()->layer_descs()) {
            ui::views::LayerTree::LayerDesc row;
            row.id = d.id;
            row.name = d.name;
            row.visible = d.visible;
            row.active = d.active;
            layers.push_back(std::move(row));
          }
          return layers;
        }());
        self_test_mark("ogr-ok");
        self_test_mark(city_pack ? "china-city-ok" : "china-plp-ok");
      }
    }
    // Pan tool must activate without crash (Map tab).
    if (!browser.run_tool_command("view.pan")) {
      self_test_detach_maps(browser);
      return 40;
    }
    {
      content::ViewHost* host = browser.edit_view_host();
      tool::Interaction* cur =
          host && host->workspace() ? host->workspace()->stack().current()
                                    : nullptr;
      if (!cur || std::strcmp(cur->id(), "view.pan") != 0) {
        self_test_detach_maps(browser);
        return 41;
      }
      content::InputEvent pan_down{};
      pan_down.kind = content::InputEvent::Kind::kLDown;
      pan_down.x_px = 40;
      pan_down.y_px = 40;
      content::InputEvent pan_move{};
      pan_move.kind = content::InputEvent::Kind::kMouseMove;
      pan_move.x_px = 70;
      pan_move.y_px = 55;
      content::InputEvent pan_up{};
      pan_up.kind = content::InputEvent::Kind::kLUp;
      pan_up.x_px = 70;
      pan_up.y_px = 55;
      if (!host->dispatch_input(pan_down) || !host->dispatch_input(pan_move) ||
          !host->dispatch_input(pan_up)) {
        self_test_detach_maps(browser);
        return 42;
      }
      self_test_mark("pan-ok");
    }
    // Wheel-to-cursor must change overlay scale (not view-center zoom).
    {
      content::ViewHost* host = browser.edit_view_host();
      const double scale0 = browser.document()->scale();
      content::InputEvent wheel{};
      wheel.kind = content::InputEvent::Kind::kWheel;
      wheel.x_px = 40;
      wheel.y_px = 40;
      wheel.wheel = 120;
      if (!host || !host->dispatch_input(wheel) ||
          std::fabs(browser.document()->scale() - scale0) < 1e-9) {
        self_test_detach_maps(browser);
        return 47;
      }
      const render::rhi::CameraMatrices ortho =
          browser.scene3d()->camera_matrices_ortho(800.f, 600.f);
      if (ortho.kind != render::rhi::CameraKind::kOrtho) {
        self_test_detach_maps(browser);
        return 48;
      }
      self_test_mark("wheel-cursor-ok");
    }
    // FlyCube orbit camera matrices must track chrome yaw/pitch.
    {
      const float yaw_after = browser.scene3d()->yaw();
      const render::rhi::CameraMatrices cam =
          browser.scene3d()->camera_matrices(1.333f);
      if (cam.kind != render::rhi::CameraKind::kPerspective ||
          std::fabs(yaw_after - app::kScene3dDefaultYaw) < 0.001f) {
        self_test_detach_maps(browser);
        return 27;
      }
      // View matrix must not be identity after orbit.
      bool view_moved = false;
      for (int i = 0; i < 16; ++i) {
        const float ident = (i % 5 == 0) ? 1.f : 0.f;
        if (std::fabs(cam.view[i] - ident) > 1e-4f) {
          view_moved = true;
          break;
        }
      }
      if (!view_moved) {
        self_test_detach_maps(browser);
        return 28;
      }
      if (scene &&
          scene->attach_mode() ==
              ui::views::MapViewport::AttachMode::kFlyCube &&
          scene->rhi_device()) {
        // Optional atmosphere exercise: demo on for self-test only; normal
        // launches leave ocean/cloud disabled.
        browser.scene3d()->enable_atmosphere_demo();
        self_test_mark("atmosphere-demo");
        if (!browser.scene3d()->present_gpu(
                static_cast<render::rhi::Device*>(scene->rhi_device()), 64,
                64)) {
          std::fprintf(stderr, "FlyCube present_gpu after orbit failed\n");
          self_test_detach_maps(browser);
          return 29;
        }
        self_test_mark("flycube-camera-ok");
      } else {
        self_test_mark("flycube-skipped");
      }
    }
    // Layout smoke: bounds non-negative, children inside parents.
    std::vector<std::string> layout_issues;
    const int layout_fails =
        ui::views::collect_layout_violations(browser.contents_view(),
                                             &layout_issues);
    self_test_mark("layout-checked");
    if (layout_fails > 0) {
      for (const std::string& issue : layout_issues) {
        std::fprintf(stderr, "layout smoke: %s\n", issue.c_str());
      }
      self_test_mark("layout-fail");
      self_test_detach_maps(browser);
      return 30;
    }
    ui::views::MapViewport* map_pane = browser.map_viewport();
    if (!map_pane || map_pane->bounds().width <= 0 ||
        map_pane->bounds().height <= 0) {
      self_test_detach_maps(browser);
      return 31;
    }
    self_test_mark("map-bounds-ok");
    // Child HWND must track View bounds in the top-level client space
    // (realize_native parents to Widget HWND; sync_native_bounds uses abs).
    if (HWND map_hwnd = map_pane->native_view()) {
      if (!IsWindow(map_hwnd)) {
        self_test_detach_maps(browser);
        return 35;
      }
      map_pane->sync_native_bounds();
      RECT wr = {};
      GetWindowRect(map_hwnd, &wr);
      POINT tl = {wr.left, wr.top};
      ScreenToClient(browser.hwnd(), &tl);
      const ui::views::Rect& vb = map_pane->bounds();
      const int tol = 2;
      if (tl.x < vb.x - tol || tl.x > vb.x + tol || tl.y < vb.y - tol ||
          tl.y > vb.y + tol) {
        std::fprintf(stderr,
                     "map hwnd origin (%ld,%ld) vs view (%d,%d)\n", tl.x, tl.y,
                     vb.x, vb.y);
        self_test_detach_maps(browser);
        return 33;
      }
      const int hw = wr.right - wr.left;
      const int hh = wr.bottom - wr.top;
      if (hw < vb.width - tol || hw > vb.width + tol ||
          hh < vb.height - tol || hh > vb.height + tol) {
        std::fprintf(stderr, "map hwnd size %dx%d vs view %dx%d\n", hw, hh,
                     vb.width, vb.height);
        self_test_detach_maps(browser);
        return 34;
      }
    } else {
      self_test_detach_maps(browser);
      return 35;
    }
    self_test_mark("hwnd-sync-ok");
    if (ui::views::View* root_view = browser.contents_view()) {
      if (ui::views::View* menu = root_view->child_at(0)) {
        if (menu->bounds().height <
            ui::views::dip_to_px(22, 1.f)) {
          self_test_detach_maps(browser);
          return 32;
        }
      }
    }
    self_test_mark("pass");
    self_test_detach_maps(browser);
    self_test_mark("detached");
    return 0;
  }
  return browser.run_loop();
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::GpuMain(params.argc, params.argv);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.browser_main = &BrowserMain;
  params.gpu_main = &GpuMain;
  params.renderer_main = &content::RendererMain;
  const int rc = content::ContentMain(params);
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
