// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/app/stdafx.h"
#include "legacy/app/shell/showcase/map2d.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "base/trace/log/frame_log.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gis/model/envelope.h"
#include "gis/model/map/map.h"
#include "legacy/app/core/smtapp.h"
#include "legacy/app/shell/showcase/host.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "legacy/render/test/paint_test_host.h"
#include "ogrsf_frmts.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace legacy_app {
namespace {

constexpr int kW = 640;
constexpr int kH = 480;
constexpr const char* kBmpLeaf = "legacy-map2d-showcase-china.bmp";
constexpr const char* kMarkLeaf = "legacy-map2d-showcase-mark.txt";
constexpr const char* kTraceLeaf = "legacy-map2d-showcase-china.trace.json";
constexpr const char* kPerfLeaf = "legacy-map2d-showcase-perf.json";
constexpr const char* kLogTag = "legacy-map2d-showcase";

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

void showcase_mark(const char* step) {
  write_showcase_mark(kMarkLeaf, kLogTag, step);
}

HWND create_showcase_hwnd() {
  return create_showcase_popup(L"SmartGisLegacyMap2dShowcase",
                               L"legacy-map2d-showcase", kW, kH);
}

std::string find_china_sample() {
  return legacy_render::detail::find_china_vector_sample();
}

bool bmp_has_carto_signal(const char* path) {
  HBITMAP bmp =
      static_cast<HBITMAP>(LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0,
                                      LR_LOADFROMFILE | LR_CREATEDIBSECTION));
  if (!bmp) {
    return false;
  }
  BITMAP bm = {};
  if (GetObject(bmp, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 ||
      bm.bmHeight <= 0) {
    DeleteObject(bmp);
    return false;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bm.bmWidth;
  bmi.bmiHeader.biHeight = -bm.bmHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  const int pixels = bm.bmWidth * bm.bmHeight;
  std::string bits(static_cast<size_t>(pixels) * 4, '\0');
  HDC hdc = GetDC(nullptr);
  const int got = GetDIBits(hdc, bmp, 0, static_cast<UINT>(bm.bmHeight),
                            bits.data(), &bmi, DIB_RGB_COLORS);
  ReleaseDC(nullptr, hdc);
  DeleteObject(bmp);
  if (got <= 0) {
    return false;
  }
  int landish = 0;
  int oceanish = 0;
  int samples = 0;
  for (int i = 0; i < pixels; i += 8) {
    const auto* p = reinterpret_cast<const unsigned char*>(bits.data() + i * 4);
    const unsigned b = p[0];
    const unsigned g = p[1];
    const unsigned r = p[2];
    ++samples;
    const bool ocean = (abs(static_cast<int>(r) - 170) < 25 &&
                        abs(static_cast<int>(g) - 211) < 25 &&
                        abs(static_cast<int>(b) - 223) < 25) ||
                       (b > r + 15 && b > 140 && g > 120 && r < 200);
    if (ocean) {
      ++oceanish;
      continue;
    }
    if ((r > 160 && g > 160 && b > 120) || (g > r + 5 && g > 100) ||
        (r > 40 && g > 40 && b > 40)) {
      ++landish;
    }
  }
  const float land_f =
      samples > 0 ? static_cast<float>(landish) / samples : 0.f;
  const float ocean_f =
      samples > 0 ? static_cast<float>(oceanish) / samples : 1.f;
  std::fprintf(stderr, "legacy-map2d-showcase: signal land=%.3f ocean=%.3f\n",
               land_f, ocean_f);
  std::fflush(stderr);
  return land_f > 0.05f && ocean_f < 0.95f;
}

}  // namespace

void dump_showcase_perf(double delay_ms, double zoom_ms, double paint_ms,
                        double save_ms, double total_ms,
                        const char* paint_path) {
  char trace_path[MAX_PATH] = {};
  char perf_path[MAX_PATH] = {};
  const bool have_trace =
      app::detail::exe_sidecar_path_a(trace_path, MAX_PATH, kTraceLeaf);
  const bool have_perf =
      app::detail::exe_sidecar_path_a(perf_path, MAX_PATH, kPerfLeaf);

  // Write Chrome Trace JSON directly (do not rely on SetEnvironmentVariable +
  // getenv for SMT_TRACE_DUMP 鈥?CRT environ and Win32 env can diverge).
  if (const char* env = std::getenv("SMT_TRACE_DUMP"); env && env[0]) {
    base::trace::maybe_dump_tracing_to_env();
  } else if (have_trace) {
    std::ofstream out(trace_path, std::ios::binary);
    if (out) {
      out << base::trace::process_trace().dump();
    }
    std::fprintf(stderr, "legacy-map2d-showcase: trace=%s\n", trace_path);
  }

  const auto events = base::trace::process_trace().snapshot_events();
  const auto phases = base::trace::rollup_trace_phases(events);
  for (const auto& p : phases) {
    std::fprintf(stderr,
                 "legacy-map2d-showcase: phase name=%s n=%llu avg_ms=%.2f "
                 "p99_ms=%.2f max_ms=%.2f\n",
                 p.name.c_str(), static_cast<unsigned long long>(p.count),
                 p.avg_us / 1000.0, p.p99_us / 1000.0, p.max_us / 1000.0);
  }
  const auto lines =
      base::trace::format_trace_frame_log_lines(events, "gdi.", "RenderMap");
  for (const auto& line : lines) {
    std::fprintf(stderr, "legacy-map2d-showcase: frame %s\n",
                 line.text.c_str());
  }
  std::fflush(stderr);

  const char* path_label =
      (paint_path && paint_path[0]) ? paint_path : "unknown";
  if (have_perf) {
    std::ofstream out(perf_path, std::ios::binary);
    if (out) {
      out << "{\n"
          << "  \"mode\": \"china\",\n"
          << "  \"viewport\": {\"w\": " << kW << ", \"h\": " << kH << "},\n"
          << "  \"delay_init_ms\": " << delay_ms << ",\n"
          << "  \"zoom_to_rect_ms\": " << zoom_ms << ",\n"
          << "  \"render_map_ms\": " << paint_ms << ",\n"
          << "  \"save_image_ms\": " << save_ms << ",\n"
          << "  \"total_ms\": " << total_ms << ",\n"
          << "  \"paint_path\": \"" << path_label << "\",\n"
          << "  \"trace_events\": " << events.size() << ",\n"
          << "  \"frame_lines\": " << lines.size() << "\n"
          << "}\n";
    }
    std::fprintf(stderr, "legacy-map2d-showcase: perf=%s\n", perf_path);
  }
  std::fprintf(stderr,
               "legacy-map2d-showcase: timing delay=%.1fms zoom=%.1fms "
               "paint=%.1fms save=%.1fms total=%.1fms path=%s events=%zu\n",
               delay_ms, zoom_ms, paint_ms, save_ms, total_ms, path_label,
               events.size());
  std::fflush(stderr);
}

int run_map2d_showcase_china(app::SmtApp& app) {
  const auto t_all = Clock::now();
  char mark_path[MAX_PATH] = {};
  if (app::detail::exe_sidecar_path_a(mark_path, MAX_PATH, kMarkLeaf)) {
    DeleteFileA(mark_path);
  }
  showcase_mark("china");

  // Arm process_trace so host GDI paint spans land in the dump.
  base::trace::maybe_init_tracing_from_env();
  if (!base::trace::tracing_enabled()) {
    base::trace::set_tracing_enabled(true);
  }

  // Init() already ran (styles / datasource). DelayInit bootstraps MapMgr for
  // interactive shell; paint uses a dedicated SmtMap like gdi_map_paint_test
  // so catalog / spatial-filter state cannot wipe the shot.
  const auto t_delay = Clock::now();
  if (!app.DelayInit()) {
    showcase_mark("delay-init-fail");
    return 10;
  }
  const double delay_ms = ms_since(t_delay);
  showcase_mark("delay-init");

  GDALAllRegister();
  const std::string sample = find_china_sample();
  if (sample.empty()) {
    showcase_mark("sample-missing");
    return 55;
  }
  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpenEx(sample.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr,
                 nullptr, nullptr));
  if (!ds || ds->GetLayerCount() < 1) {
    showcase_mark("sample-open-fail");
    if (ds) {
      GDALClose(ds);
    }
    return 55;
  }

  gis::SmtMap map;
  int appended = 0;
  for (int li = 0; li < ds->GetLayerCount(); ++li) {
    OGRLayer* lyr = ds->GetLayer(li);
    if (!lyr) {
      continue;
    }
    if (map.AddLayer(lyr)) {
      ++appended;
    }
  }
  if (appended < 1) {
    showcase_mark("map-empty");
    GDALClose(ds);
    return 55;
  }
  showcase_mark("china-ok");
  std::fprintf(stderr, "legacy-map2d-showcase: sample=%s layers=%d\n",
               sample.c_str(), appended);
  std::fflush(stderr);

  HWND hwnd = create_showcase_hwnd();
  if (!hwnd) {
    showcase_mark("hwnd-fail");
    GDALClose(ds);
    return 57;
  }
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd);

#ifdef _DEBUG
  HMODULE dll = LoadLibraryA("legacy_render_d.dll");
#else
  HMODULE dll = LoadLibraryA("legacy_render.dll");
#endif
  if (!dll) {
    showcase_mark("dll-fail");
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 57;
  }
  auto create = reinterpret_cast<render::_CreateRenderDevice>(
      GetProcAddress(dll, "CreateRenderDevice"));
  auto destroy = reinterpret_cast<render::_DestroyRenderDevice>(
      GetProcAddress(dll, "DestroyRenderDevice"));
  if (!create || !destroy) {
    showcase_mark("exports-fail");
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 57;
  }

  render::LPRENDERDEVICE dev = nullptr;
  if (create(dll, dev) != 0 || !dev) {
    showcase_mark("create-fail");
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 57;
  }
  if (dev->Init(hwnd, "legacy-map2d-showcase") != SMT_ERR_NONE) {
    showcase_mark("init-fail");
    destroy(dev);
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 57;
  }
  showcase_mark("device-ok");
  if (dev->Resize(0, 0, kW, kH) != SMT_ERR_NONE) {
    showcase_mark("resize-fail");
    destroy(dev);
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 57;
  }

  Smt2DRenderPra pra = {};
  // Carto showcase: debug MBR / vertex crosses dominate china multipolygon
  // paint and are not part of the visual gates.
  pra.bShowMBR = false;
  pra.bShowPoint = false;
  pra.lPointRaduis = 4;
  dev->SetRenderPra(pra);

  // China lon/lat framing (Views kChinaLonLatExtent). Prefer layer envelope
  // when it looks like China; otherwise force the known box.
  constexpr float kMinX = 73.f;
  constexpr float kMinY = 18.f;
  constexpr float kMaxX = 135.f;
  constexpr float kMaxY = 54.f;
  map.CalEnvelope();
  gis::Envelope env;
  map.get_envelope(env);
  base::fRect frt;
  frt.lb.x = kMinX;
  frt.lb.y = kMinY;
  frt.rt.x = kMaxX;
  frt.rt.y = kMaxY;
  if (env.is_init() && env.MinX >= 50.0 && env.MaxX <= 160.0 &&
      env.MinY >= 0.0 && env.MaxY <= 70.0) {
    frt.lb.x = static_cast<float>(env.MinX);
    frt.lb.y = static_cast<float>((std::max)(env.MinY, 15.0));
    frt.rt.x = static_cast<float>(env.MaxX);
    frt.rt.y = static_cast<float>(env.MaxY);
  }
  std::fprintf(stderr, "legacy-map2d-showcase: fit=(%.2f,%.2f)-(%.2f,%.2f)\n",
               frt.lb.x, frt.lb.y, frt.rt.x, frt.rt.y);
  std::fflush(stderr);

  const auto t_zoom = Clock::now();
  if (dev->ZoomToRect(&map, frt, /*bRealTime=*/true) != SMT_ERR_NONE) {
    showcase_mark("zoom-fail");
    destroy(dev);
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 57;
  }
  const double zoom_ms = ms_since(t_zoom);
  showcase_mark("fit-ok");

  // ZoomToRect(realtime) already fills m_smtMapRenderBuf (worker publish, or
  // UI fallback inside ReRenderMapRealTime). A second BeginRender(clear) +
  // RenderMap wiped that front and paid china paint twice (~2s + ~2.5s).
  // Prefer SaveImage of the Zoom result; sync paint only if carto signal fails.
  char bmp_a[MAX_PATH] = {};
  if (!app::detail::exe_sidecar_path_a(bmp_a, MAX_PATH, kBmpLeaf)) {
    showcase_mark("sidecar-fail");
    destroy(dev);
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 56;
  }

  const auto t_paint = Clock::now();
  const char* paint_path = "worker";
  DeleteFileA(bmp_a);
  const auto t_save0 = Clock::now();
  if (dev->SaveImage(bmp_a, render::MRD_BL_MAP) != SMT_ERR_NONE) {
    showcase_mark("save-fail");
    destroy(dev);
    DestroyWindow(hwnd);
    GDALClose(ds);
    return 54;
  }
  double save_ms = ms_since(t_save0);
  showcase_mark("bmp-ok");

  if (!bmp_has_carto_signal(bmp_a)) {
    showcase_mark("bmp-blank-retry-sync");
    paint_path = "sync_fallback";
    for (int i = 0; i < 8000; ++i) {
      if (dev->BeginRender(render::MRD_BL_MAP, true) == SMT_ERR_NONE) {
        const int rr = dev->RenderMap(&map, R2_COPYPEN);
        dev->EndRender(render::MRD_BL_MAP);
        if (rr == SMT_ERR_NONE) {
          break;
        }
      }
      ::Sleep(1);
    }
    GdiFlush();
    DeleteFileA(bmp_a);
    const auto t_save1 = Clock::now();
    if (dev->SaveImage(bmp_a, render::MRD_BL_MAP) != SMT_ERR_NONE) {
      showcase_mark("save-fail");
      destroy(dev);
      DestroyWindow(hwnd);
      GDALClose(ds);
      return 54;
    }
    save_ms += ms_since(t_save1);
    if (!bmp_has_carto_signal(bmp_a)) {
      showcase_mark("bmp-blank");
      destroy(dev);
      DestroyWindow(hwnd);
      GDALClose(ds);
      return 54;
    }
  }
  const double paint_ms = ms_since(t_paint);
  showcase_mark(std::strcmp(paint_path, "worker") == 0 ? "paint-reuse-ok"
                                                       : "paint-sync-ok");

  showcase_mark("signal-ok");
  std::fprintf(stderr, "legacy-map2d-showcase: wrote %s\n", bmp_a);
  std::fprintf(stderr, "legacy-map2d-showcase: PASS mode=china path=%s\n",
               paint_path);
  std::fflush(stderr);

  dump_showcase_perf(delay_ms, zoom_ms, paint_ms, save_ms, ms_since(t_all),
                     paint_path);

  GDALClose(ds);
  showcase_mark("destory-ok");
  ::TerminateProcess(::GetCurrentProcess(), 0);
  return 0;
}

}  // namespace legacy_app
