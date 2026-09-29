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
#include "gis/vista/world/terrain/dem_frame.h"
#include "legacy/app/core/smtapp.h"
#include "legacy/app/shell/showcase/host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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

// Burn engine id into the visual top of a bottom-up BGR24 buffer (GDI TextOut).
void burn_engine_label_bgr(unsigned char *bgr_bottom_up, int w, int h,
                           const wchar_t *label) {
  if (!bgr_bottom_up || !label || w <= 0 || h <= 0) {
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
  // Bottom-up 鈫?top-down copy.
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
  const int bar_h = 32;
  HBRUSH bar = CreateSolidBrush(RGB(0, 0, 0));
  RECT rc = {0, 0, w, bar_h};
  FillRect(mem, &rc, bar);
  DeleteObject(bar);
  SetBkMode(mem, TRANSPARENT);
  SetTextColor(mem, RGB(255, 255, 0));
  HFONT font =
      CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  FIXED_PITCH | FF_MODERN, L"Consolas");
  HGDIOBJ old_font = font ? SelectObject(mem, font) : nullptr;
  TextOutW(mem, 8, 6, label, static_cast<int>(wcslen(label)));
  if (old_font) {
    SelectObject(mem, old_font);
  }
  if (font) {
    DeleteObject(font);
  }
  // Top-down 鈫?bottom-up copy back.
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
  int unique = 0;
  unsigned char seen[64][3] = {};
  const int step = (std::max)(1, (w * h) / 8000);
  for (int i = 0; i < w * h; i += step) {
    const int y = i / w;
    const int x = i % w;
    const unsigned char *p =
        pixels.data() + static_cast<size_t>(y) * stride + x * 3;
    const unsigned b = p[0];
    const unsigned g = p[1];
    const unsigned r = p[2];
    if (!(r < 12 && g < 12 && b < 12) && !(r > 245 && g > 245 && b > 245)) {
      ++non_flat;
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
  return non_flat > 40 && unique >= 2;
}

} // namespace

int run_scene3d_showcase_china(app::SmtApp &app) {
  char mark_path[MAX_PATH] = {};
  if (app::detail::exe_sidecar_path_a(mark_path, MAX_PATH, kMarkLeaf)) {
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

  HWND hwnd = create_showcase_hwnd(stereo_engine_title_w());
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

  const float yaw = gis::kDemDefaultOrbitYaw;
  const float pitch = 0.4f;
  const float distance = 3.2f;
  for (int i = 0; i < 3; ++i) {
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
  showcase_mark("present-ok");

  char bmp_a[MAX_PATH] = {};
  if (!app::detail::exe_sidecar_path_a(bmp_a, MAX_PATH, kBmpLeaf)) {
    showcase_mark("sidecar-fail");
    destroy(view);
    DestroyWindow(hwnd);
    return 56;
  }
  char bmp_backend[MAX_PATH] = {};
  char backend_leaf[64] = {};
  sprintf_s(backend_leaf, "legacy-scene3d-showcase-china-%s.bmp", backend);
  const bool have_backend_path =
      app::detail::exe_sidecar_path_a(bmp_backend, MAX_PATH, backend_leaf) != 0;
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
  burn_engine_label_bgr(bgr.data(), kW, kH, stereo_engine_title_w());
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

  // Avoid MFC/BCG DLL_PROCESS_DETACH deadlock (same as --map2d-showcase).
  showcase_mark("destory-ok");
  ::TerminateProcess(::GetCurrentProcess(), 0);
  return 0;
}

} // namespace legacy_app
