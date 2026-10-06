// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_UI_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_UI_H_

#include "app/views/app/cmdline/views_launch_options.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {

class Browser;

namespace detail {

// Deterministic horizon theme for visual gates (default dark; UI_THEME).
void apply_ui_harness_theme();

// Kill DrawHost present timers and drain queued WM_TIMER (no DrawHost detach).
void stop_ui_map_present(Browser& browser);

// Layout + Invalidate + pump so PrintWindow sees finished horizon.
void force_ui_shell_repaint(Browser& browser);

// UI_SHOWCASE_TIMED_MS / UI_SHOWCASE_LINGER_MS; 0 = no linger.
int ui_showcase_linger_ms();

// Optional MAP2D_FPS_BENCH_MS loop (writes map2d-fps-bench.txt).
void run_horizon_map2d_fps_bench(Browser& browser);

// Capture shell HWND client for horizon visual gates. Window-DC / PrintWindow
// for horizon. When |map_hwnd| is a FlyCube DXGI present surface, composites
// that client via screen BitBlt (CAPTUREBLT) so WS_EX_NOREDIRECTIONBITMAP
// holes are not written as the map. Never desktop-blits the whole shell.
bool capture_ui_shell_bmp(HWND hwnd,
                          const wchar_t* filename,
                          HWND map_hwnd = nullptr,
                          HWND hud_hwnd = nullptr);

// One --ui-showcase pass in three stages. Harness and capability slots may
// run other work between them.

// Mode-specific tab / catalog / inspector / scene panel orchestration before
// layout gate + BMP. Does not own product horizon widgets — only drives them.
void apply_ui_scenario_panels(Browser& browser, UiShowcaseMode mode);

// Runs layout / overlap / shell anomaly checks. On failure (or UI_FORENSICS)
// dumps under out/ui_forensics/. Returns 0 on pass, else showcase exit code 30.
int run_ui_layout_gate(Browser& browser, UiShowcaseMode mode);

// After layout gate: linger, keep Scene on 3D, composite FlyCube present
// pixels into the shell BMP, HUD FPS mark, horizon BMP write, stop timers.
// Returns 0 on success, else showcase exit code (54/55).
int run_ui_present_capture(Browser& browser, UiShowcaseMode mode);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_UI_H_
