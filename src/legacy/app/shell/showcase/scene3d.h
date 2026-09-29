// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_SHOWCASE_SCENE3D_H_
#define LEGACY_APP_SHELL_SHOWCASE_SCENE3D_H_

namespace app {
class SmtApp;
}

namespace legacy_app {

// Headless leftover stereo + BMP sidecar for SmartGis.exe --scene3d-showcase.
// Mirrors Views atmosphere showcase capture via smt_stereo_hwnd_* (no MDI).
// Backend: D3D11 (default) or OpenGL (SMT_STEREO_API=OpenGL /
// SMT_SCENE3D_SHOWCASE_D3D=0). Window title + BMP title bar show the engine id.
// Writes legacy-scene3d-showcase-china.bmp and
// legacy-scene3d-showcase-china-{gl|d3d}.bmp next to the exe.
// |app| is the CSmartGisApp (inherits SmtApp) that already ran Init().
// Returns process exit code (0 = PASS). Caller should TerminateProcess.
int run_scene3d_showcase_china(app::SmtApp &app);

} // namespace legacy_app

#endif // LEGACY_APP_SHELL_SHOWCASE_SCENE3D_H_
