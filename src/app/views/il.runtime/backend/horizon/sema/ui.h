// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_UI_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_UI_H_

#include <string>

namespace app {

class Browser;

namespace detail {

// Shell UI capture modes driven by Interact / ScenarioRegistry (string modes).
enum class UiMode {
  kNone,
  kShell,     // Map Edit tab + dark horizon BMP
  kData,      // Map Data tab
  kScene,     // Scene3D tab
  kCatalog,   // Catalog Maps page + Map tab
  kInteract,  // Cycle Map→Data→Scene→Map then capture
};

UiMode ui_mode_from_name(const std::string& mode);
const char* ui_mode_name(UiMode mode);

// Deterministic horizon theme for visual gates (default dark; UI_THEME).
void apply_ui_harness_theme();

// Kill DrawHost present timers and drain queued WM_TIMER (no DrawHost detach).
void stop_ui_map_present(Browser& browser);

// Layout + Invalidate + pump so PrintWindow sees finished horizon.
void force_ui_shell_repaint(Browser& browser);

// Timed / linger env (ui-*-ms harness switches); 0 = no linger.
int ui_linger_ms();

// Optional MAP2D_FPS_BENCH_MS loop (writes map2d-fps-bench.txt).
void run_horizon_map2d_fps_bench(Browser& browser);

// One UI capture pass in three stages. Harness and capability slots may
// run other work between them.

// Mode-specific tab / catalog / inspector / scene panel orchestration before
// layout gate + BMP. Does not own product horizon widgets — only drives them.
void apply_ui_scenario_panels(Browser& browser, UiMode mode);

// Runs layout / overlap / shell anomaly checks. On failure (or UI_FORENSICS)
// dumps under out/ui_forensics/. Returns 0 on pass, else exit code 30.
int run_ui_layout_gate(Browser& browser, UiMode mode);

// After layout gate: linger, keep Scene on 3D, composite FlyCube present
// pixels into the shell BMP, HUD FPS mark, horizon BMP write, stop timers.
// Returns 0 on success, else exit code (54/55).
int run_ui_present_capture(Browser& browser, UiMode mode);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_UI_H_
