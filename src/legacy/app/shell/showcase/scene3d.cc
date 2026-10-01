// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"
#include "legacy/app/shell/showcase/scene3d.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "gis/vista/world/terrain/dem/dem_frame.h"
#include "legacy/app/core/smtapp.h"
#include "legacy/app/shell/showcase/host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

namespace legacy_app {
namespace {

constexpr int kW = 640;
constexpr int kH = 480;
constexpr const char *kBmpLeaf = "legacy-scene3d-showcase-china.bmp";
constexpr const char *kMarkLeaf = "legacy-scene3d-showcase-mark.txt";
constexpr const char *kLogTag = "legacy-scene3d-showcase";

bool stereo_backend_is_d3d() {
  // Default D3D11; OpenGL is opt-in via SMT_STEREO_API=OpenGL / SHOWCASE_D3D=0.
  if (const char *api = std::getenv("SMT_STEREO_API")) {
    if (_stricmp(api, "OpenGL") == 0) {
      return false;
    }
    if (_stricmp(api, "Direct3D") == 0) {
      return true;
    }
  }
  if (const char *flag = std::getenv("SMT_SCENE3D_SHOWCASE_D3D")) {
    if (flag[0] == '0' || flag[0] == 'n' || flag[0] == 'N') {
      return false;
    }
    if (flag[0] == '1' || flag[0] == 'y' || flag[0] == 'Y') {
      return true;
    }
  }
  return true;
}

const char *stereo_backend_id() {
  return stereo_backend_is_d3d() ? "d3d" : "gl";
}

const wchar_t *stereo_engine_title_w() {
  return stereo_backend_is_d3d()
             ? L"legacy-scene3d-d3d | Legacy Scene3D (D3D11)"
             : L"legacy-scene3d-gl | Legacy Scene3D (OpenGL)";
}

const char *stereo_engine_title_a() {
  return stereo_backend_is_d3d()
             ? "legacy-scene3d-d3d | Legacy Scene3D (D3D11)"
             : "legacy-scene3d-gl | Legacy Scene3D (OpenGL)";
}

using CreateFn = void *(*)(HWND);
using DestroyFn = void (*)(void *);
using ResizeFn = int (*)(void *, int, int);
using PresentFn = int (*)(void *, float, float, float);
using CaptureFn = int (*)(void *, unsigned char *, int, int);

void showcase_mark(const char *step) {
  write_showcase_mark(kMarkLeaf, kLogTag, step);
}

HWND create_showcase_hwnd(const wchar_t *title) {
  return create_showcase_popup(L"SmartGisLegacyScene3dShowcase",
                               title ? title : L"legacy-scene3d-showcase", kW,
                               kH);
}

// Forensic linger: forward OS pan/wheel into orbit present so record frames
// change (blank WndProc previously left a frozen SwapBuffers image).
struct Scene3dLingerNav {
  void *view = nullptr;
  PresentFn present = nullptr;
  float yaw = 0.f;
  float pitch = 0.55f;
  float distance = 2.5f;
  bool dragging = false;
  int origin_x = 0;
  int origin_y = 0;
  DWORD last_present_tick = 0;
};

Scene3dLingerNav g_scene3d_nav;

void present_scene3d_hwnd() {
  if (!g_scene3d_nav.view || !g_scene3d_nav.present) {
    return;
  }
  (void)g_scene3d_nav.present(g_scene3d_nav.view, g_scene3d_nav.yaw,
                              g_scene3d_nav.pitch, g_scene3d_nav.distance);
}

// Drain queued WM_MOUSEMOVE so slow present (~sub-1s) does not apply a long
// backlog of orbit deltas after OS inject returns (end-frame near_black spike).
void coalesce_mouse_move(HWND hwnd, LPARAM* lparam, WPARAM* wparam) {
  MSG peek = {};
  while (::PeekMessageW(&peek, hwnd, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE)) {
    *lparam = peek.lParam;
    *wparam = peek.wParam;
  }
}

LRESULT CALLBACK scene3d_linger_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      g_scene3d_nav.dragging = true;
      g_scene3d_nav.origin_x = GET_X_LPARAM(lparam);
      g_scene3d_nav.origin_y = GET_Y_LPARAM(lparam);
      ::SetCapture(hwnd);
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (!g_scene3d_nav.dragging || (wparam & MK_LBUTTON) == 0) {
        return 0;
      }
      coalesce_mouse_move(hwnd, &lparam, &wparam);
      if ((wparam & MK_LBUTTON) == 0) {
        return 0;
      }
      const int x = GET_X_LPARAM(lparam);
      const int y = GET_Y_LPARAM(lparam);
      int dx = x - g_scene3d_nav.origin_x;
      int dy = y - g_scene3d_nav.origin_y;
      // One clamped step per drained batch — jump origin to latest so queued
      // inject floods cannot integrate unbounded yaw.
      if (dx > 20) {
        dx = 20;
      } else if (dx < -20) {
        dx = -20;
      }
      if (dy > 20) {
        dy = 20;
      } else if (dy < -20) {
        dy = -20;
      }
      g_scene3d_nav.origin_x = x;
      g_scene3d_nav.origin_y = y;
      g_scene3d_nav.yaw += static_cast<float>(dx) * 0.003f;
      g_scene3d_nav.pitch += static_cast<float>(dy) * 0.003f;
      if (g_scene3d_nav.pitch < 0.15f) {
        g_scene3d_nav.pitch = 0.15f;
      }
      if (g_scene3d_nav.pitch > 1.2f) {
        g_scene3d_nav.pitch = 1.2f;
      }
      const DWORD now = ::GetTickCount();
      if (g_scene3d_nav.last_present_tick != 0 &&
          (now - g_scene3d_nav.last_present_tick) < 90) {
        return 0;
      }
      g_scene3d_nav.last_present_tick = now;
      present_scene3d_hwnd();
      return 0;
    }
    case WM_LBUTTONUP: {
      if (g_scene3d_nav.dragging) {
        g_scene3d_nav.last_present_tick = 0;
        present_scene3d_hwnd();
      }
      g_scene3d_nav.dragging = false;
      ::ReleaseCapture();
      return 0;
    }
    case WM_MOUSEWHEEL: {
      const short z_delta = GET_WHEEL_DELTA_WPARAM(wparam);
      if (z_delta < 0) {
        g_scene3d_nav.distance *= 1.04f;
      } else {
        g_scene3d_nav.distance *= 0.96f;
      }
      // Keep china slab in frame for HWND record / near_black gates.
      if (g_scene3d_nav.distance < 1.6f) {
        g_scene3d_nav.distance = 1.6f;
      }
      if (g_scene3d_nav.distance > 6.5f) {
        g_scene3d_nav.distance = 6.5f;
      }
      g_scene3d_nav.last_present_tick = ::GetTickCount();
      present_scene3d_hwnd();
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void attach_scene3d_linger_nav(HWND hwnd, void *view, PresentFn present,
                               float yaw, float pitch, float distance) {
  g_scene3d_nav.view = view;
  g_scene3d_nav.present = present;
  g_scene3d_nav.yaw = yaw;
  g_scene3d_nav.pitch = pitch;
  g_scene3d_nav.distance = distance;
  g_scene3d_nav.dragging = false;
  ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(scene3d_linger_wnd_proc));
}

// Burn engine / strategy labels into the visual top of a bottom-up BGR24
// buffer (GDI TextOut). Line 0 = strategy (if any); line 1 = engine title.
void burn_engine_label_bgr(unsigned char *bgr_bottom_up, int w, int h,
                           const wchar_t *engine_label,
                           const wchar_t *strategy_label) {
  if (!bgr_bottom_up || w <= 0 || h <= 0) {
    return;
  }
  if ((!engine_label || !engine_label[0]) &&
      (!strategy_label || !strategy_label[0])) {
    return;
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h; // top-down DIB
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 24;
  bi.bmiHeader.biCompression = BI_RGB;
  void *bits = nullptr;
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return;
  }
  HDC mem = CreateCompatibleDC(screen);
  HBITMAP dib = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);
    return;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  const int stride = ((w * 3 + 3) / 4) * 4;
  auto *dst = static_cast<unsigned char *>(bits);
  // Bottom-up → top-down copy.
  for (int y = 0; y < h; ++y) {
    const unsigned char *src =
        bgr_bottom_up + static_cast<size_t>(h - 1 - y) * w * 3;
    unsigned char *row = dst + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; ++x) {
      row[x * 3 + 0] = src[x * 3 + 0];
      row[x * 3 + 1] = src[x * 3 + 1];
      row[x * 3 + 2] = src[x * 3 + 2];
    }
  }
  const bool have_strategy = strategy_label && strategy_label[0];
  const bool have_engine = engine_label && engine_label[0];
  const int lines = (have_strategy ? 1 : 0) + (have_engine ? 1 : 0);
  const int bar_h = 8 + lines * 22;
  // Opaque red bar so strategy text is unmistakable on DEM / black sky.
  HBRUSH bar = CreateSolidBrush(RGB(160, 0, 0));
  RECT rc = {0, 0, w, bar_h};
  FillRect(mem, &rc, bar);
  DeleteObject(bar);
  SetBkMode(mem, TRANSPARENT);
  HFONT font =
      CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  FIXED_PITCH | FF_MODERN, L"Consolas");
  HGDIOBJ old_font = font ? SelectObject(mem, font) : nullptr;
  int text_y = 6;
  if (have_strategy) {
    SetTextColor(mem, RGB(0, 255, 128));
    TextOutW(mem, 8, text_y, strategy_label,
             static_cast<int>(wcslen(strategy_label)));
    text_y += 22;
  }
  if (have_engine) {
    SetTextColor(mem, RGB(255, 255, 0));
    TextOutW(mem, 8, text_y, engine_label,
             static_cast<int>(wcslen(engine_label)));
  }
  if (old_font) {
    SelectObject(mem, old_font);
  }
  if (font) {
    DeleteObject(font);
  }
  // Top-down → bottom-up copy back.
  for (int y = 0; y < h; ++y) {
    const unsigned char *row = dst + static_cast<size_t>(y) * stride;
    unsigned char *out = bgr_bottom_up + static_cast<size_t>(h - 1 - y) * w * 3;
    for (int x = 0; x < w; ++x) {
      out[x * 3 + 0] = row[x * 3 + 0];
      out[x * 3 + 1] = row[x * 3 + 1];
      out[x * 3 + 2] = row[x * 3 + 2];
    }
  }
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
}

// Human-readable parallel strategy from env (or SMT_SCENE3D_SHOWCASE_STRATEGY).
std::wstring strategy_label_w() {
  if (const char *custom = std::getenv("SMT_SCENE3D_SHOWCASE_STRATEGY")) {
    if (custom[0]) {
      wchar_t wide[160] = {};
      MultiByteToWideChar(CP_UTF8, 0, custom, -1, wide, 160);
      if (!wide[0]) {
        MultiByteToWideChar(CP_ACP, 0, custom, -1, wide, 160);
      }
      return wide;
    }
  }
  auto on = [](const char *key, bool default_on) {
    const char *e = std::getenv(key);
    if (!e || !e[0]) {
      return default_on;
    }
    return !(e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' ||
             e[0] == 'F');
  };
  const bool fj = on("SMT_RHI3D_FRAME_JOB", true);
  const bool prep = on("SMT_RHI3D_PREP_PARALLEL", true);
  const bool def = on("SMT_RHI3D_D3D_DEFERRED", true);
  const bool is_d3d = stereo_backend_is_d3d();
  wchar_t buf[128] = {};
  if (!fj && !prep && (!is_d3d || !def)) {
    swprintf_s(buf, L"strategy: serial (FJ=0 PREP=0%s)",
               is_d3d ? L" DEF=0" : L"");
  } else if (fj && !prep && (!is_d3d || !def)) {
    swprintf_s(buf, L"strategy: P1 FrameJob");
  } else if (fj && prep && (!is_d3d || !def)) {
    swprintf_s(buf, L"strategy: P1+P2 FrameJob+Prep%s",
               is_d3d ? L" (DEF=0)" : L"");
  } else if (fj && prep && is_d3d && def) {
    swprintf_s(buf, L"strategy: P1+P2+P3 FrameJob+Prep+Deferred");
  } else {
    swprintf_s(buf, L"strategy: FJ=%d PREP=%d DEF=%d", fj ? 1 : 0,
               prep ? 1 : 0, def ? 1 : 0);
  }
  return buf;
}

HMODULE load_legacy_render() {
#ifdef _DEBUG
  const char *names[] = {"legacy_render_d.dll", "legacy_render.dll", nullptr};
#else
  const char *names[] = {"legacy_render.dll", "legacy_render_d.dll", nullptr};
#endif
  char dir[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, dir, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    if (char *slash = strrchr(dir, '\\')) {
      *(slash + 1) = '\0';
    }
  } else {
    dir[0] = '\0';
  }
  for (const char **p = names; *p; ++p) {
    if (dir[0]) {
      char full[MAX_PATH] = {};
      if (sprintf_s(full, "%s%s", dir, *p) > 0) {
        if (HMODULE m = LoadLibraryA(full)) {
          return m;
        }
      }
    }
    if (HMODULE m = LoadLibraryA(*p)) {
      return m;
    }
  }
  return nullptr;
}

bool write_bgr24_bmp(const char *path, const unsigned char *bgr_bottom_up,
                     int w, int h) {
  if (!path || !bgr_bottom_up || w <= 0 || h <= 0) {
    return false;
  }
  const int stride24 = ((w * 3 + 3) / 4) * 4;
  BITMAPFILEHEADER fh = {};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(stride24) * h;
  BITMAPINFOHEADER bi = {};
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = w;
  bi.biHeight = h; // bottom-up
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  bi.biSizeImage = static_cast<DWORD>(stride24) * h;

  FILE *out = nullptr;
  if (fopen_s(&out, path, "wb") != 0 || !out) {
    return false;
  }
  std::fwrite(&fh, sizeof(fh), 1, out);
  std::fwrite(&bi, sizeof(bi), 1, out);
  if (stride24 == w * 3) {
    std::fwrite(bgr_bottom_up, 1, static_cast<size_t>(stride24) * h, out);
  } else {
    std::vector<unsigned char> row(static_cast<size_t>(stride24), 0);
    for (int y = 0; y < h; ++y) {
      const unsigned char *src = bgr_bottom_up + static_cast<size_t>(y) * w * 3;
      std::memcpy(row.data(), src, static_cast<size_t>(w) * 3);
      std::fwrite(row.data(), 1, static_cast<size_t>(stride24), out);
    }
  }
  std::fclose(out);
  return true;
}

bool bmp_has_visible_signal(const char *path) {
  FILE *in = nullptr;
  if (fopen_s(&in, path, "rb") != 0 || !in) {
    return false;
  }
  BITMAPFILEHEADER fh = {};
  BITMAPINFOHEADER bi = {};
  if (std::fread(&fh, sizeof(fh), 1, in) != 1 ||
      std::fread(&bi, sizeof(bi), 1, in) != 1 || fh.bfType != 0x4D42) {
    std::fclose(in);
    return false;
  }
  const int w = bi.biWidth;
  const int h = bi.biHeight < 0 ? -bi.biHeight : bi.biHeight;
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
  int non_flat = 0;
  int landish = 0;
  int unique = 0;
  unsigned char seen[64][3] = {};
  const int step = (std::max)(1, (w * h) / 8000);
  int sampled = 0;
  for (int i = 0; i < w * h; i += step) {
    const int y = i / w;
    const int x = i % w;
    const unsigned char *p =
        pixels.data() + static_cast<size_t>(y) * stride + x * 3;
    const unsigned b = p[0];
    const unsigned g = p[1];
    const unsigned r = p[2];
    ++sampled;
    if (!(r < 12 && g < 12 && b < 12) && !(r > 245 && g > 245 && b > 245)) {
      ++non_flat;
    }
    // Match testing/tools/loop/score/bmp.py score_legacy_scene3d_china landish.
    if ((g > r + 8 && g > b + 5 && g > 70) ||
        (r > 90 && g > 80 && b < 130 && r + g > b * 2) ||
        (r > 140 && g > 140 && (r > g ? r - g : g - r) < 40 && r + g > b * 1.5)) {
      ++landish;
    }
    bool found = false;
    for (int u = 0; u < unique; ++u) {
      if (seen[u][0] == static_cast<unsigned char>(r) &&
          seen[u][1] == static_cast<unsigned char>(g) &&
          seen[u][2] == static_cast<unsigned char>(b)) {
        found = true;
        break;
      }
    }
    if (!found && unique < 64) {
      seen[unique][0] = static_cast<unsigned char>(r);
      seen[unique][1] = static_cast<unsigned char>(g);
      seen[unique][2] = static_cast<unsigned char>(b);
      ++unique;
    }
  }
  // Reject HUD-only black frames (FPS/compass): need DEM/land wash.
  const float land_f =
      sampled > 0 ? static_cast<float>(landish) / static_cast<float>(sampled)
                  : 0.f;
  return non_flat > 40 && unique >= 2 && land_f > 0.04f;
}

} // namespace

int run_scene3d_showcase_china(app::SmtApp &app) {
  char mark_path[MAX_PATH] = {};
  if (app::detail::exe_capture_path_a(mark_path, MAX_PATH, kMarkLeaf)) {
    DeleteFileA(mark_path);
  }
  const bool use_d3d = stereo_backend_is_d3d();
  const char *backend = stereo_backend_id();
  showcase_mark(use_d3d ? "china-d3d" : "china-gl");
  std::fprintf(stderr, "legacy-scene3d-showcase: engine=%s\n",
               stereo_engine_title_a());
  std::fflush(stderr);

  // Bootstrap sample paths / singletons; stereo seed also opens china itself.
  if (!app.DelayInit()) {
    showcase_mark("delay-init-fail");
    // Continue 鈥?seed_sample_map_into_scene resolves ../data independently.
  } else {
    showcase_mark("delay-init");
  }

  HWND hwnd = create_showcase_hwnd(L"legacy-scene3d-showcase");
  if (!hwnd) {
    showcase_mark("hwnd-fail");
    return 57;
  }
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd);
  showcase_mark("hwnd-ok");

  HMODULE dll = load_legacy_render();
  if (!dll) {
    showcase_mark("dll-fail");
    DestroyWindow(hwnd);
    return 57;
  }
  auto create =
      reinterpret_cast<CreateFn>(GetProcAddress(dll, "smt_stereo_hwnd_create"));
  auto destroy = reinterpret_cast<DestroyFn>(
      GetProcAddress(dll, "smt_stereo_hwnd_destroy"));
  auto resize =
      reinterpret_cast<ResizeFn>(GetProcAddress(dll, "smt_stereo_hwnd_resize"));
  auto present = reinterpret_cast<PresentFn>(
      GetProcAddress(dll, "smt_stereo_hwnd_present"));
  auto capture = reinterpret_cast<CaptureFn>(
      GetProcAddress(dll, "smt_stereo_hwnd_capture_bgr24"));
  if (!create || !destroy || !resize || !present || !capture) {
    showcase_mark("exports-fail");
    DestroyWindow(hwnd);
    return 57;
  }

  void *view = create(hwnd);
  if (!view) {
    showcase_mark("create-fail");
    DestroyWindow(hwnd);
    return 51;
  }
  showcase_mark(use_d3d ? "device-ok-d3d" : "device-ok-gl");
  if (!resize(view, kW, kH)) {
    showcase_mark("resize-fail");
    destroy(view);
    DestroyWindow(hwnd);
    return 51;
  }

  // Closer + slightly steeper so china DEM fills the frame (near_black gate
  // <0.85). Visual review: reduce solid black void around the mesh.
  const float yaw = gis::kDemDefaultOrbitYaw;
  float pitch = 0.62f;
  float distance = 2.15f;
  int present_count = 5;
  if (const char *pc = std::getenv("SMT_SCENE3D_SHOWCASE_PRESENT_COUNT")) {
    const int v = std::atoi(pc);
    if (v > 0 && v <= 600) {
      present_count = v;
    }
  }
  LARGE_INTEGER qpf = {};
  LARGE_INTEGER t0 = {};
  LARGE_INTEGER t1 = {};
  QueryPerformanceFrequency(&qpf);
  QueryPerformanceCounter(&t0);
  for (int i = 0; i < present_count; ++i) {
    char frame_mark[32];
    sprintf_s(frame_mark, "present-%d", i);
    showcase_mark(frame_mark);
    if (!present(view, yaw, pitch, distance)) {
      showcase_mark("present-fail");
      ::TerminateProcess(::GetCurrentProcess(), 52);
      return 52;
    }
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  QueryPerformanceCounter(&t1);
  const double present_ms =
      (qpf.QuadPart > 0)
          ? (1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) /
             static_cast<double>(qpf.QuadPart))
          : 0.0;
  char perf_leaf[MAX_PATH] = {};
  if (app::detail::exe_capture_path_a(perf_leaf, MAX_PATH,
                                      "legacy-scene3d-showcase-perf.json")) {
    if (FILE *pf = nullptr; fopen_s(&pf, perf_leaf, "wb") == 0 && pf) {
      std::fprintf(pf,
                   "{\"backend\":\"%s\",\"present_count\":%d,"
                   "\"present_ms\":%.3f,\"ms_per_present\":%.3f}\n",
                   backend, present_count, present_ms,
                   present_count > 0 ? present_ms / present_count : 0.0);
      std::fclose(pf);
    }
  }
  showcase_mark("present-ok");

  char bmp_a[MAX_PATH] = {};
  if (!app::detail::exe_capture_path_a(bmp_a, MAX_PATH, kBmpLeaf)) {
    showcase_mark("sidecar-fail");
    destroy(view);
    DestroyWindow(hwnd);
    return 56;
  }
  char bmp_backend[MAX_PATH] = {};
  char backend_leaf[64] = {};
  sprintf_s(backend_leaf, "legacy-scene3d-showcase-china-%s.bmp", backend);
  const bool have_backend_path =
      app::detail::exe_capture_path_a(bmp_backend, MAX_PATH, backend_leaf);
  DeleteFileA(bmp_a);
  if (have_backend_path) {
    DeleteFileA(bmp_backend);
  }
  // Extra present so the front buffer is fresh before glReadPixels.
  present(view, yaw, pitch, distance);
  std::vector<unsigned char> bgr(
      static_cast<size_t>(kW) * static_cast<size_t>(kH) * 3u, 0);
  if (!capture(view, bgr.data(), kW, kH)) {
    showcase_mark("save-fail");
    ::TerminateProcess(::GetCurrentProcess(), 54);
    return 54;
  }
  const std::wstring strategy = strategy_label_w();
  burn_engine_label_bgr(bgr.data(), kW, kH, stereo_engine_title_w(),
                        strategy.c_str());
  std::fprintf(stderr, "legacy-scene3d-showcase: strategy=%ls\n",
               strategy.c_str());
  if (!write_bgr24_bmp(bmp_a, bgr.data(), kW, kH)) {
    showcase_mark("save-fail");
    ::TerminateProcess(::GetCurrentProcess(), 54);
    return 54;
  }
  if (have_backend_path) {
    write_bgr24_bmp(bmp_backend, bgr.data(), kW, kH);
  }
  showcase_mark("bmp-ok");

  if (!bmp_has_visible_signal(bmp_a)) {
    showcase_mark("bmp-blank");
    // Skip destroy 鈥?D3D teardown under debug CRT can HEAP-break after present;
    // same TerminateProcess policy as the PASS path / --map2d-showcase.
    ::TerminateProcess(::GetCurrentProcess(), 54);
    return 54;
  }
  showcase_mark("signal-ok");
  std::fprintf(stderr, "legacy-scene3d-showcase: engine=%s\n",
               stereo_engine_title_a());
  std::fprintf(stderr, "legacy-scene3d-showcase: wrote %s\n", bmp_a);
  if (have_backend_path) {
    std::fprintf(stderr, "legacy-scene3d-showcase: wrote %s\n", bmp_backend);
  }
  std::fprintf(stderr, "legacy-scene3d-showcase: PASS mode=china backend=%s\n",
               backend);
  std::fflush(stderr);

  // Keep a fresh SwapBuffers on the HWND for linger OS-inject / screen
  // BitBlt recording (PrintWindow of GL/D3D surfaces is often all-black).
  if (present(view, yaw, pitch, distance)) {
    showcase_mark("hwnd-present-ok");
  } else {
    showcase_mark("hwnd-present-fail");
  }

  attach_scene3d_linger_nav(hwnd, view, present, yaw, pitch, distance);
  showcase_mark("nav-ok");
  // Avoid MFC/BCG DLL_PROCESS_DETACH deadlock (same as --map2d-showcase).
  showcase_linger_from_env("SMT_SCENE3D_SHOWCASE_LINGER_MS");
  // Do not destroy(view) here: GL ICD teardown before TerminateProcess still
  // leaves the next harness round flaky (bmp_missing / exit 0xFFFFFFFF ~2s).
  // Harness settle after kill (see testing/tools/loop/kill.py) covers relaunch.
  ::TerminateProcess(::GetCurrentProcess(), 0);
  return 0;
}

} // namespace legacy_app
